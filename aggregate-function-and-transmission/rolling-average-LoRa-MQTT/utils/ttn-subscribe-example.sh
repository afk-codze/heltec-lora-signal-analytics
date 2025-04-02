#!/bin/bash

BROKER="eu1.cloud.thethings.network"
PORT=1883
USERNAME="heltec-lora-signal-analytics@ttn"
PASSWORD="YOUR_API_KEY"  # Replace this with your real API key
TOPIC="v3/heltec-lora-signal-analytics@ttn/devices/+/up"

mosquitto_sub -h "$BROKER" -p "$PORT" -u "$USERNAME" -P "$PASSWORD" -t "$TOPIC"