#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>
extern int errno;
int port;
int ok;
int started;
GtkWidget *overlay;
GtkWidget *window;
GtkWidget *image;
GtkWidget *label;
GtkWidget *frame;
GtkWidget *entry;
GtkWidget *text_box;
GtkWidget *vbox;
static gchar *input_text = NULL;
int input_ready = 0;

static void create_window();
void background();
void add_text_on_image(const char *text);
void on_text_entered(GtkWidget *entry, gpointer user_data);
void update_text_on_image(const char *text);
void create_vbox();
void create_frame();
void create_label(const char *text);
void create_text_box();
void update_text_on_image_answer(const char *text);
void update_text_on_image_answer_1(const char *text);
void update_text_on_image_answer_2(const char *text);
int main(int argc, char *argv[])
{
  gtk_init(&argc, &argv);
  create_window();
  background();
  int sd;
  struct sockaddr_in server;
  char buf[10];
  if (argc != 3)
  {
    printf("Syntax: %s <server_adress> <port>\n", argv[0]);
    return -1;
  }
  port = atoi(argv[2]);
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("Socket error().\n");
    return errno;
  }
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(port);

  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Connect error().\n");
    return errno;
  }

  int size;
  if (recv(sd, &size, sizeof(int), 0) == -1)
  {
    perror("[client]Error at recv() from server.\n");
    return errno;
  }
  char rules[size];
  if (recv(sd, &rules, size + 1, 0) == -1)
  {
    perror("[client]Error at  recv() from server.\n");
    return errno;
  }

  if (strcmp(rules, "Timeout expired. Game started!\n") == 0)
  {
    
    started = 1;
    add_text_on_image(rules);
     gtk_main();
    exit(0);
  }
  else
  { 
    add_text_on_image(rules);
    fflush(stdout);
    while (!input_ready)
    {
      gtk_main_iteration_do(FALSE);
    }
    strcpy(buf, input_text);
    size = strlen(buf) + 1;
    if (send(sd, &size, sizeof(int), 0) == -1)
    {
      perror("[client]Error at send() to server.\n");
      return errno;
    }
    if (send(sd, &buf, strlen(buf) + 1, 0) == -1)
    {
      perror("[client]Error at send() to server.\n");
      return errno;
    }
    int answear;
    int i = 0;
    while (1)
    {
      i++;
      if (recv(sd, &size, sizeof(int), 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }
      char buffer[size];
      if (recv(sd, &buffer, size, 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }
     
      input_ready = 0;
      gtk_entry_set_text(GTK_ENTRY(text_box), "");
      update_text_on_image(buffer);
      while (!input_ready)
      {
        gtk_main_iteration_do(FALSE);
      }
      fflush(stdout);
      char aux[10];
      strcpy(aux, input_text);
      answear = atoi(aux);
      if (send(sd, &answear, sizeof(int), 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }
      if (recv(sd, &size, sizeof(int), 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }
      char buffer1[size];
      if (recv(sd, &buffer1, size, 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }

      update_text_on_image_answer(buffer1);

      if (strcmp(buffer1, "Exit\n") == 0)
      {
        ok = 1;
        break;
      }

      if (i == 3)
        break;
    }

    if (ok == 0)
    {
      if (recv(sd, &size, sizeof(int), 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }
      char buffer2[size];
      if (recv(sd, &buffer2, size, 0) == -1)
      {
        perror("[client]Error recv() from server.\n");
        return errno;
      }
      update_text_on_image_answer_1(buffer2);
    }

    close(sd);
  }
  gtk_main();
  exit(0);
  return 0;

}
static void create_window()
{
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "QUIZZ GAME");
  gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  overlay = gtk_overlay_new();
  gtk_container_add(GTK_CONTAINER(window), overlay);

  gtk_widget_show_all(window);
}

void background()
{
  image = gtk_image_new_from_file("fundal.jpg");
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), image);
  gtk_widget_show_all(window);
}
void create_vbox()
{
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
}

void create_frame()
{
  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width(GTK_CONTAINER(frame), 20);
}

void create_label(const char *text)
{
  label = gtk_label_new(NULL);
  char *formatted_text = g_strdup_printf("<b><span size='%d'>%s</span></b>", 14000, text);
  gtk_label_set_markup(GTK_LABEL(label), formatted_text);
  g_free(formatted_text);
}

void create_text_box()
{
  text_box = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text_box), "");
  gtk_widget_set_size_request(text_box, 400, 50);
  g_signal_connect(G_OBJECT(text_box), "activate", G_CALLBACK(on_text_entered), NULL);
}

void add_text_on_image(const char *text)
{
  create_frame();
  create_label(text);
  gtk_container_add(GTK_CONTAINER(frame), label);
  gtk_widget_show_all(frame);

  if (started == 0)
  {
    create_vbox();
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    create_text_box();
    gtk_box_pack_start(GTK_BOX(vbox), text_box, FALSE, FALSE, 0);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), vbox);
  }
  else
  { 
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), frame);
  }

  gtk_widget_show_all(window);
}

void on_text_entered(GtkWidget *entry, gpointer user_data)
{
  const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
  if (text != NULL && strlen(text) > 0)
  {
    input_text = g_strdup(text);
    input_ready = 1;
  }
}
void update_text_on_image(const char *text)
{
  gchar *formatted_text = g_strdup_printf("<b><span size='%d'>%s</span></b>", 14000, text);
  gtk_label_set_markup(GTK_LABEL(label), formatted_text);
  g_free(formatted_text);
  gtk_widget_show(label);
  gtk_widget_show(text_box);
  gtk_widget_grab_focus(text_box);
}
void update_text_on_image_answer(const char *text)
{
  gchar *formatted_text = g_strdup_printf("<b><span size='%d'>%s</span></b>", 14000, text);
  gtk_label_set_markup(GTK_LABEL(label), formatted_text);
  g_free(formatted_text);
  gtk_widget_hide(text_box);
  gtk_widget_show(label);
  GDateTime *start_time = g_date_time_new_now_local();
  while (TRUE)
  {
    GDateTime *current_time = g_date_time_new_now_local();
    GTimeSpan elapsed = g_date_time_difference(current_time, start_time);
    if (elapsed >= 3000000)
      break;
    g_date_time_unref(current_time);

    while (gtk_events_pending())
      gtk_main_iteration();
  }
  g_date_time_unref(start_time);
}
void update_text_on_image_answer_1(const char *text)
{
  gchar *formatted_text = g_strdup_printf("<b><span size='%d'>%s</span></b>", 14000, text);
  gtk_label_set_markup(GTK_LABEL(label), formatted_text);
  g_free(formatted_text);
  gtk_widget_hide(text_box);
  gtk_widget_show(label);
}
