FROM ubuntu:24.04

RUN apt update
RUN apt upgrade -y

RUN apt install python3 python3-pip python3-venv python3-dev -y

# RUN ln -s /usr/bin/python3.14 /usr/bin/python

WORKDIR /app
COPY server.py /app
COPY /templates /app/templates

RUN python3 -m venv venv

COPY requirements.txt /app/requirements.txt
RUN ./venv/bin/pip install --no-cache-dir -r requirements.txt

ENV PATH="/app/venv/bin:$PATH"
ENV FLASKENV="PROD"

EXPOSE 3344
EXPOSE 4512

ENTRYPOINT ["python", "server.py"]