#ifndef WEBSERVICE_H
#define WEBSERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "Pages.h"

// WiFi credentials
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// Create a web server on port 80
WebServer server(80);

// Function prototypes
void handleRoot();
void handleNotFound();

#endif