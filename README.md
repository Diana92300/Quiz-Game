# Networked Quiz Game (Client & Server)

This project implements a simple quiz game over TCP sockets with a GTK-based C client and a multi-threaded C server using SQLite for data storage.

## Overview

* **Server (Server.c):**

  * Listens for incoming connections on port 5114
  * Manages up to 100 clients with POSIX threads
  * Loads questions and answers from an SQLite database (`intrebari.db`)
  * Records player usernames and scores in `jucatori.db`
  * Enforces timeouts, quiz rules, and sends ranking at the end

* **Client (Client.c):**

  * GTK3 GUI displays rules, questions, and feedback
  * Connects to the server via TCP, sends username and answers
  * Renders text overlays on a background image (`fundal.jpg`)

## Repository Structure

```text
├── Client.c           # GTK-based quiz client
├── Server.c           # Multi-threaded quiz server
├── fundal.jpg         # Background image for client GUI
├── intrebari.db       # SQLite DB with quiz questions
├── jucatori.db        # SQLite DB storing player scores
```

## Requirements

* GCC or compatible C compiler
* GTK 3 development libraries
* SQLite3 development libraries
* pthreads (POSIX threads)

## Features

* Multithreaded server handles each client in a detached thread.
* Timeout for game start (30 s) and per-question response (15 s).
* SQLite integration for persistent storage of questions and scores.
* GTK3 client GUI with text overlays and input field.
* Clean shutdown, ranking broadcast, and record cleanup.
