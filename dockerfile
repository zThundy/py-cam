FROM python:3.9-slim

WORKDIR /app
COPY server.py /app

COPY requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

EXPOSE 3344
EXPOSE 4512

CMD ["python", "server.py"]