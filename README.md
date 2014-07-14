MySMTP
====

MySMTP is an implementation of the SMTP RFC.

This project provides a basic SMTP client and an SMTP Relay to allow somone to send mails provided he or she has an Internet connection.

Features
----
Client features:

+ Can connect ton any SMTP server (tested successfully with Gmail and Hotmail)
+ Can use SSL with TLSv1 to encrypt the connection
+ Handles login/password authentication if necessary

Server features:

+ Relays mails via the Gmail SMTP
+ Can accept multiple client connections at once
+ Uses SSL

How-to-build
----

You can build directly the 2 binaries (client and server) with the `build.sh` script located at the root of this repository.

There is a DEBUG variant of this project. It provides useful log infos. You may disable it by removing the `MYSMTP_DEBUG` on top of the build script.

The SSL library is required, but a precompiled version is provided with this project.

How-to-run
----

 + Client

You can use the client alone, and connect to whatever SMTP you want. The client has the following syntax:

    ./client <IP or host> <port> [ssl]
    ./client smtp.live.com 587 ssl

Here, we specified the Hotmail SMTP hostname `smtp.live.com`, it listens on port `587` and uses `ssl`.
If the server doesn't require SSL, you can ommit the `ssl` parameter

+ Server

You can start the SMTP relay with the following command:

    ./server <IP> <port>
    ./server 192.168.0.5 4567

Here we start an SMTP relay which will listen on IP `192.168.0.5` and uses the port `4567`.

You can then connect to your relay with the client:

    ./client 192.168.0.5 4567


