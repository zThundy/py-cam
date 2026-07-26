FROM python:3.14-slim

# RUN apt update
# RUN apt upgrade -y

# RUN apt install python3 python3-pip python3-venv python3-dev -y

# RUN ln -s /usr/bin/python3.14 /usr/bin/python
ENV PIP_ROOT_USER_ACTION=ignore

WORKDIR /app
COPY server.py /app
COPY /templates /app/templates
COPY requirements.txt /app/requirements.txt

# RUN python3 -m venv venv
RUN apt update && \
    apt upgrade -y && \
    apt install ffmpeg -y --force-yes && \
    pip install --no-cache-dir -r requirements.txt

# ENV PATH="/app/venv/bin:$PATH"
# ENV FLASKENV="PROD"

EXPOSE 3344
EXPOSE 4512

CMD ["python", "server.py"]