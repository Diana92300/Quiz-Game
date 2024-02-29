#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>

#define PORT 5114
extern int errno;

typedef struct thData
{
    int idThread;
    int cl;
    int status; // 2 pt in joc, 1 pt  a parasit jocul si 0 pt a raspuns la toate intrebarile 

} thData;
thData clients[100];
struct sockaddr_in server;
struct sockaddr_in from;

pthread_t th[100];
int sd, on;

typedef struct
{
    sqlite3 *db_ptr;
    pthread_mutex_t db_mutex;

} DataBase;

DataBase data_base_manager, data_base_manager_1;

int nr_clients;  // nr clienti activi
int all_clients; // nr total clienti
int started;     // nr de clienti acceptati in joc
int flag;
int time_expired = -1;
int ok = 0;

static void *treat(void *arg);
void initialization_socket();
void start_communication();
void data_base_initialization(DataBase *data_base_manager, const char *data_base_name);
void close_data_base(DataBase *data_base_manager);
void insert_in_data_base(void *arg, DataBase *data_base_manager, char *buffer);
void send_the_rules(void *arg);
void answear(void *arg);
void extract_the_question_from_data_base(char *question, int nr_question, char *correct_answear);
void score_update(thData tdL);
void delete_records();
void *send_question(void *arg);
void ranking();

int main()
{
    initialization_socket();
    start_communication();
}

static void *treat(void *arg)
{
   
    fflush(stdout);
    pthread_detach(pthread_self());
    send_the_rules((struct thData *)arg);
    if (!flag)
        answear((struct thData *)arg);
    close((intptr_t)arg);
    return (NULL);
}

void initialization_socket()
{
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Error at socket().\n");
        exit(0);
    }
    on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Error at bind().\n");
        exit(0);
    }
            

    if (listen(sd, 2) == -1)
    {
        perror("[server]Error at listen().\n");
        exit(0);
    }
}

void start_communication()
{
    int i = 0;
    struct timeval timeout;
    fd_set set;
    int rv = 1;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    int client;
    thData *td;
    int aux = sd + 1;

    printf("[serverWait]We were waiting at the port %d...\n", PORT);
    fflush(stdout);

    while (1)
    {
        socklen_t length = sizeof(from);
        FD_ZERO(&set);
        FD_SET(sd, &set);
        rv = select(aux, &set, NULL, NULL, &timeout);

        if (rv == -1)
        {
            perror("Error at select");
            break;
        }
        else if (rv == 0&&!started)
        {
            printf("Timeout expired. Game started!\n");
            fflush(stdout);
            started = 1;
            nr_clients = i - 1;
            time_expired = 1;
             pthread_t question_thread;
            if (pthread_create(&question_thread, NULL, send_question, NULL) != 0) {
                perror("Failed to create question thread");
            }
            pthread_detach(question_thread);
        }
        else
        if(rv>0)
        {if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
        {
            perror("[server]Error at accept().\n");
            continue;
        }
        td = (struct thData *)malloc(sizeof(struct thData));
        clients[i].cl = client;
        clients[i].idThread = i;
        clients[i].status = 2;
        all_clients = i;
        td->idThread = i++;
        td->cl = client;
        td->status = 2;
        pthread_create(&th[i], NULL, &treat, td);
        }
    }
}

void data_base_initialization(DataBase *data_base_manager, const char *data_base_name)
{

    int ret = 0;
    ret = sqlite3_open(data_base_name, &(data_base_manager->db_ptr));
    if (ret != SQLITE_OK)
    {
        printf("Error opening database\n");
        exit(0);
    }
    if (pthread_mutex_init(&(data_base_manager->db_mutex), NULL) != 0)
    {
        printf("Error initializing mutex\n");
        exit(0);
    }
}

void close_data_base(DataBase *data_base_manager)
{
    sqlite3_close(data_base_manager->db_ptr);
    pthread_mutex_destroy(&(data_base_manager->db_mutex));
}

void insert_in_data_base(void *arg, DataBase *data_base_manager, char *buffer)
{
    struct thData tdL;
    tdL = *((struct thData *)arg);
    pthread_mutex_lock(&(data_base_manager->db_mutex));
    char sql_stmt[400];
    char *errMesg = 0;
    snprintf(sql_stmt, sizeof(sql_stmt), "INSERT INTO jucatori VALUES ('%d','%s', 0)", tdL.idThread, buffer);
    int ret = 0;
    ret = sqlite3_exec(data_base_manager->db_ptr, sql_stmt, 0, 0, &errMesg);
    if (ret != SQLITE_OK)
    {
        printf("[Thread %d]Eroare SQL: %s\n", tdL.idThread, errMesg);
        sqlite3_free(errMesg);
        pthread_mutex_unlock(&(data_base_manager->db_mutex));
        exit(0);
    }
    printf("[Thread %d]The insertion in the database was successfully completed.\n", tdL.idThread);
    pthread_mutex_unlock(&(data_base_manager->db_mutex));
}

void send_the_rules(void *arg)
{
    struct thData tdL;
    tdL = *((struct thData *)arg);
    int size;
    if (1 != started)
    {
        char rules[500] = "\0";
        strcpy(rules, "Rules\n For each question you must choose\n the number corresponding to the correct answer.\n If you want to leave the game, \n you must write -1.\n Every question is mandatory.\n Write your username:");
        size = 226;
   
        if (send(tdL.cl, &size, sizeof(int), 0) == -1)
        {
            perror("[server]Error recv() from server.\n");
            exit(0);
        }

        if (send(tdL.cl, &rules, size, 0) == -1)
        {
            perror("[server]Error recv() from server.\n");
            exit(0);
        }
    }
    else
    {
        char over[100] = "\0";
        strcpy(over, "Timeout expired. Game started!\n");
        size = 33;
        flag = 1;
        if (send(tdL.cl, &size, sizeof(int), 0) == -1)
        {
            perror("[server]Error recv() from server.\n");
            exit(0);
        }

        if (send(tdL.cl, &over, size, 0) == -1)
        {
            perror("[server]Error recv() from server.\n");
            exit(0);
        }
    }
}
void answear(void *arg)
{
    struct thData tdL;
    tdL = *((struct thData *)arg);
    int size;
    if (recv(tdL.cl, &size, sizeof(int), 0) == -1)
    {
        perror("[server]Error recv() from server.\n");
        exit(0);
    }

    char buffer[size];
    if (recv(tdL.cl, &buffer, size, 0) == -1)
    {
        perror("[server]Error recv() from server.\n");
        exit(0);
    }
    printf("[Thread %d]The message has been received...%s\n", tdL.idThread, buffer);

    data_base_initialization(&data_base_manager, "jucatori.db");
    insert_in_data_base((struct thData *)arg, &data_base_manager, buffer);
}

void extract_the_question_from_data_base(char *question, int nr_question, char *correct_answear)
{

    data_base_initialization(&data_base_manager_1, "intrebari.db");
    pthread_mutex_lock(&(data_base_manager_1.db_mutex));
    sqlite3_stmt *stmt;
    char sql_stmt[200], answear[100];
    int ret = 0;
    snprintf(sql_stmt, sizeof(sql_stmt), "SELECT intrebare,raspuns,varianta1,varianta2,varianta3,varianta4 FROM intrebari WHERE id=%d", nr_question);
    ret = sqlite3_prepare_v2(data_base_manager_1.db_ptr, sql_stmt, -1, &stmt, 0);
    if ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        strcpy(answear, (const char *)sqlite3_column_text(stmt, 1));
        strcat(question, (const char *)sqlite3_column_text(stmt, 0));
        strcat(question, "\n");
        strcat(question, "(1)");
        strcat(question, (const char *)sqlite3_column_text(stmt, 2));
        strcat(question, "\n");
        strcat(question, "(2)");
        strcat(question, (const char *)sqlite3_column_text(stmt, 3));
        strcat(question, "\n");
        strcat(question, "(3)");
        strcat(question, (const char *)sqlite3_column_text(stmt, 4));
        strcat(question, "\n");
        strcat(question, "(4)");
        strcat(question, (const char *)sqlite3_column_text(stmt, 5));
        strcat(question, "\n");
        if (strcmp(answear, (const char *)sqlite3_column_text(stmt, 2)) == 0)
            strcpy(correct_answear, "1");
        else if (strcmp(answear, (const char *)sqlite3_column_text(stmt, 3)) == 0)
            strcpy(correct_answear, "2");
        else if (strcmp(answear, (const char *)sqlite3_column_text(stmt, 4)) == 0)
            strcpy(correct_answear, "3");
        else if (strcmp(answear, (const char *)sqlite3_column_text(stmt, 5)) == 0)
            strcpy(correct_answear, "4");
    }
    pthread_mutex_unlock(&(data_base_manager_1.db_mutex));
    close_data_base(&data_base_manager_1);
}

void score_update(thData tdL)
{
    char id_thread[10];
    pthread_mutex_lock(&(data_base_manager.db_mutex));
    char sql[100] = "\0";
    snprintf(id_thread, sizeof(id_thread), "%d", tdL.idThread);
    strcpy(sql, "UPDATE jucatori SET punctaj=punctaj+1");
    strcat(sql, " WHERE id=");
    strcat(sql, id_thread);
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(data_base_manager.db_ptr, sql, -1, &stmt1, 0);
    if (sqlite3_step(stmt1) != SQLITE_DONE)
    {
        printf("SQL eroare: %s\n", sqlite3_errmsg(data_base_manager.db_ptr));
        exit(0);
    }
    else
        printf("Valoarea a fost incrementata cu succes!\n");

    sqlite3_finalize(stmt1);
    pthread_mutex_unlock(&(data_base_manager.db_mutex));
}

void *send_question(void *arg)
{
    char question[500], is_correct[100], correct_answear[10];
    strcpy(question,"\0");
    strcpy(correct_answear,"\0");
    strcpy(is_correct,"\0");
    int size, answear;
    for (int i = 1; i <= 3; i++)
    {
        extract_the_question_from_data_base(question, i, correct_answear);
        size = strlen(question) + 1;
        for (int j = 0; j <= nr_clients; j++)
        { 
            if (clients[j].status == 2)
            {   
                if (send(clients[j].cl, &size, sizeof(int), 0) == -1)
                {
                    perror("[server]Error send() from server.\n");
                    exit(0);
                }

                if (send(clients[j].cl, question, size, 0) == -1)
                {
                    perror("[server]Error send() from server.\n");
                    exit(0);
                }
                
            }
        } 
        struct timeval timeout;
        fd_set set;
        int rv = 1;
        timeout.tv_sec =15;
        timeout.tv_usec = 0;
        int aux = sd + 1;
        for (int j = 0; j <= nr_clients; j++)
        {    
            if (clients[j].status == 2)
            {   
                FD_ZERO(&set);
                FD_SET(sd, &set);
                rv = select(aux, &set, NULL, NULL, &timeout);
                if (recv(clients[j].cl, &answear, sizeof(int), 0) == -1)
                {
                    perror("[server]Error recv() from server.\n");
                    exit(0);
                }

                if (rv == -1)
                {
                    perror("Error at select");
                    break;
                }
                else if (rv == 0)
                {
                    printf("Timeout expired.!\n");
                    fflush(stdout);
                }
                int x = atoi(correct_answear);

                if (answear == x)
                {
                    strcpy(is_correct, "Congratulations! You answered correctly \n and you have accumulated 1 point.\n");
                    score_update(clients[j]);

                    size = 84;

                    if (send(clients[j].cl, &size, sizeof(int), 0) == -1)
                    {
                        perror("[server]Error send() from server.\n");
                        exit(0);
                    }

                    if (send(clients[j].cl, is_correct, size, 0) == -1)
                    {
                        perror("[server]Error send() from server.\n");
                        exit(0);
                    }
                }
                else if (answear != -1)
                {
                    strcpy(is_correct, "You answered incorrectly!:(\n");
                    size = 38;

                    if (send(clients[j].cl, &size, sizeof(int), 0) == -1)
                    {
                        perror("[server]Error send() from server.\n");
                        exit(0);
                    }

                    if (send(clients[j].cl, is_correct, size, 0) == -1)
                    {
                        perror("[server]Error send() from server.\n");
                        exit(0);
                    }
                }
                else
                {
                    clients[j].status =1; // A parasit jocul
                    
                    strcpy(is_correct, "Exit\n");
                    size = 7;

                    if (send(clients[j].cl, &size, sizeof(int), 0) == -1)
                    {
                        perror("[server]Error send() from server.\n");
                        exit(0);
                    }

                    if (send(clients[j].cl, is_correct, size, 0) == -1)
                    {
                        perror("[server]Error send() from server.\n");
                        exit(0);
                    }
                    close(clients[j].cl);
                }

            }
            strcpy(question, "\0");
        }
        strcpy(question, "\0");
        if(i==3)
            for(int k=0;k<=nr_clients;k++)
            {  
                if(clients[k].status==2)
                    clients[k].status=0;
            }
    }
    ranking();
     return NULL;
}
void delete_records()
{
    char *errMesg = 0;
    pthread_mutex_lock(&(data_base_manager.db_mutex));
    char sql[100];
    strcpy(sql, "DELETE FROM jucatori");

    if (sqlite3_exec(data_base_manager.db_ptr, sql, 0, 0, &errMesg) != SQLITE_OK)
    {
        fprintf(stderr, "Eroare SQL: %s\n", errMesg);
        sqlite3_free(errMesg);
        sqlite3_close(data_base_manager.db_ptr);
    }

    printf("Inregistrari sterse cu succes!\n");
    pthread_mutex_unlock(&(data_base_manager.db_mutex));
}
void send_clasament(char *ranking)
{
    printf("send_classament_aic\n");
    pthread_mutex_lock(&(data_base_manager.db_mutex));
    sqlite3_stmt *stmt = NULL;
    char sql_stmt[200];
    struct order
    {
        int score, status;
        char name[20];
    } players[100], aux;
    snprintf(sql_stmt, sizeof(sql_stmt), "SELECT nume,punctaj FROM jucatori");
    int j = 0;
    sqlite3_prepare_v2(data_base_manager.db_ptr, sql_stmt, -1, &stmt, 0);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strcpy(players[j].name, (const char *)sqlite3_column_text(stmt, 0));
        players[j].score = sqlite3_column_int(stmt, 1);
        players[j].status = clients[j].status;
        j++;
    }

    for (int i = 0; i < j; i++)
        for (int k = i + 1; k < j; k++)
            if (players[i].score < players[k].score)
            {
                aux = players[i];
                players[i] = players[k];
                players[k] = aux;
            }
    char x[10];
    for (int i = 0; i < j; i++)
        if (players[i].status == 0)
        {   
            strcat(ranking, players[i].name);
            strcat(ranking, " :");
            snprintf(x, 10, "%d", players[i].score);
            strcat(ranking, x);
            strcat(ranking, "\n");
        }
        else
          if(players[i].status == 1)
          {
            strcat(ranking, players[i].name);
            strcat(ranking, " :left the game\n");
          }
    pthread_mutex_unlock(&(data_base_manager.db_mutex));
    
}
void ranking()
{
    char ranking[900] = "Ranking:\n";
    send_clasament(ranking);
    int size = strlen(ranking) + 1;
    for (int i = 0; i <= nr_clients; i++)
    {
        if(clients[i].status==0)
        {if (send(clients[i].cl, &size, sizeof(int), 0) == -1)
        {
            perror("[server]Error send() from server.\n");
            exit(0);
        }
        if (send(clients[i].cl, ranking, size, 0) == -1)
        {
            perror("[server]Error send() from server.\n");
            exit(0);}
        }
    }
    delete_records();
    close_data_base(&data_base_manager);
    exit(0);
}