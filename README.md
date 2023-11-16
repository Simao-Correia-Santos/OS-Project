# OS-Project
Academic project developed as part of the Operating Systems course in the Computer Engineering program (2022-2023)

## 🎯Goal/Description
The developed project simulate a simplified IoT environment where multiples sensors send information to a centralized point, which in turn, stores this data, generate statistics and triggers alerts when certains condition are reached. 	

![imagem geral do trabalho](https://github.com/Simao-Correia-Santos/Trabalho-SO/assets/138619513/4c34cfb2-15f1-4933-ab97-1c900cd80889)


Upon startup system, both users and sensors can send data through two named pipes, dedicated to each one. The data is received by two threads, one for the sensors and another for the users. The threads place the data in the interal queue if it isn´t full and send a signal to the dispatcher. If the internal queue is full the data from sensors is deleted and the users requests wait in the thread using a condition variable for an available slot. When a slot becomes available, it receives a signal sent by the dispatcher. 
the dispatcher, on the other hand, waits in a condition variable for a node in the internal queue if it is empty. This one removes always the first node because data from sensors is appended to the internal queue while users requests are inserted at the beginning due to their priority. The remotion and insertion of nodes in the internal queue are controlled by a semaphore.

The dispatcher then checks the value of a semaphore indicating if any worker is available, and if so it looks for the worker_id and send the information through corresponding unnamed_pipe. The worker sets is state as busy in the shared memory and processes the request. If it is a user request, the workers accesses the shared memory and fetch the information. If the data comes from the sensors, the worker update the information in the shared memory. Then the worker posts to the semaphore that synchronizing the alerts watcher and all workers, where the alert watcher is waiting in a sem_wait() to check if the values are inside the systems registered alerts.
 
Finally, the results obtained by user requests are returned to their respective user console, as well the alerts triggered, through a message queue syncronized by a semaphore. Both the startup and shutdown of the program, triggered alerts, changes in workers availability, receveid signals and processes creation are written to a log file.


## 🛠️How to test
1. Open a Linux development environment; 
1. Load all .c files and the Makefile into the same directory; 
1. Compile all files with Makefile;
1. Open as many sensors and user as you want with ./sensor {name} {time_interval} {key} {min} {max} e ./user_console {console_id}


## ✔️ Technologies Used
- ``C``
- ``Linux``
- ``VS Code``
- ``Makefile``


