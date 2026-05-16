FROM ubuntu:24.04

RUN apt-get update && apt-get install -y zlib1g

# Create and set the working directory
WORKDIR /usr/src/mtaserver

# Copy the entire project into the working directory
COPY Server.out .

# Specify the command to run your executable
CMD ["./Server.out"]
