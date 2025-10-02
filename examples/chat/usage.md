This document explains how to set up and use the chat service.
The service is designed to be self-hosted, secure, and customizable.
Ideal for organizations that value control and privacy over their communication tools and data.


# Self-hosted chat at a company
Admin does:
```bash
# login to computer hosting the chat service
ssh admin@chat.company.com

git clone https://www.github.com/Emarioo/chat-service

cd chat-service

# build chat-service
./build.py

# server will create data directories and default configs in the provided directory
# and then ask admin to optionally edit config then shutdown.
bin/chat_server ../chat-service-data

# Admin edits the config changes:
#   whether account creation on the client end is supported
#   permissions for new users
vim ../chat-service-data/config.txt

# create new accounts with temporary passwords
# clients logging in to a user with temporary passwords will get notification to change their password
bin/chat_server ../chat-service-data --create-user

# Since the config and directory exists the server starts for real
# if config has website enabled it will host that on the specified port
# if config has desktop client enabled then that may be enabled
bin/chat_server ../chat-service-data
```


Users in a browser searches `chat.company.com`.
Or runs `chat-client.exe` provided by Admins using network shared storage or whatever other mean.
(or git clone and builds the chat service altough it's best Admin distributes a specific version)

Then they login using credentials provided by Admin. Or if they are asked to create new account.
