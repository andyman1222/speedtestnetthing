# speedtestnetthing

Forgive the commit message titles.

# Run
in x64/Release/ run either of the following:

* UDPclient.exe <server IP> <Port=27016>

* TCPclient.exe <server IP> <Port=27015>

* UDPserver.exe <Port=27016>

* TCPserver.exe <Port=27015>

On the client, enter a text message and press enter. The message will be sent to the specified server, which will send the message back to the client. When the client receives the server's message, the time it took from send to receive will be displayed.

In addition: The time, number of bytes sent, original message, and received message will be inserted into TCP.csv or UDP.csv (depending on which client is run) (data from some of my own tests might already exist in the files, and the clients will append to them).

# Compile
open in Visual Studio, make sure you have C++ compilation tools, make sure Release x64 build configuration is selected (not debug), build solution
