#include "painlessMesh.h"
#include <Arduino_JSON.h>

#include <iostream>
#include <cmath>

#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
#define DEVICE "ESP32"
  #elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Configuración de la red Mesh
#define MESH_PREFIX "whateveryoulike"
#define MESH_PASSWORD "somethingSneak"
#define MESH_PORT 1515

  // WiFi AP SSID
  #define WIFI_SSID "FAMILIA_BENAVIDES"
  // WiFi password
  #define WIFI_PASSWORD "juan2004"

// Configuración de InfluxDB
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "lFwWiXtp445KCHUrv4nofpaSE3dnGpTKxtfWJPn8O3tNLcGcADrgw0NVPXiwIktFcIn5Ec9P7V3NE58s9gBOnw=="
#define INFLUXDB_ORG "16446c2508ada63c"
#define INFLUXDB_BUCKET "ESPfinal"

// Pines del LED RGB
#define RED_PIN 13
#define GREEN_PIN 14
#define BLUE_PIN 12

// Zona horaria
#define TZ_INFO "<-05>5"

// Cliente InfluxDB
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Scheduler y mesh
Scheduler userScheduler;
painlessMesh mesh;

// Variables
double temperature = 0, humidity = 0;
int redValue = 0, greenValue = 0, blueValue = 0;
int nodeNumber = 1;

// Punto de datos para InfluxDB
Point sensor("environment");

// Prototipos de funciones
void controlLedRGB(double temp);
void sendInfoToNode1(); // Función para enviar información al Nodo 1
void sendToInfluxDB(float temp, float hum);
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void conectionWifi();
void controllerWifiInfluxdb(double temp, double hum);

// Función para controlar el LED RGB
void controlLedRGB(double temp) {
/* Esta función controla el estado de un LED RGB en función de la 
   temperatura proporcionada. Si la temperatura es menor a 20°C, 
   el LED se ilumina de color azul (frío). Si la temperatura está 
   entre 20°C y 30°C, el LED se ilumina de color verde (moderado). 
   Si la temperatura es mayor o igual a 30°C, el LED se ilumina 
   de color rojo (calor). Los valores de intensidad de los colores 
   son establecidos a través de la función analogWrite().
*/
  if (temp < 20) {
    redValue = 0; greenValue = 0; blueValue = 255; // Frío
  } else if (temp >= 20 && temp < 30) {
    redValue = 0; greenValue = 255; blueValue = 0; // Moderado
  } else {
    redValue = 255; greenValue = 0; blueValue = 0; // Calor
  }

  analogWrite(RED_PIN, redValue);
  analogWrite(GREEN_PIN, greenValue);
  analogWrite(BLUE_PIN, blueValue);

}

// Conexion a Wifi
void conectionWifi(){
/* Esta función establece una conexión a la red Wi-Fi especificada 
   mediante el SSID y la contraseña configurados. Mientras no se logre 
   una conexión, imprime puntos en el monitor serial. Una vez conectada, 
   sincroniza el tiempo utilizando un servidor NTP y valida la conexión 
   con la base de datos InfluxDB. También imprime los resultados de 
   la validación en el monitor serial para depuración.
*/
  // Conectar a WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConectado a WiFi");
  // Sincronización de tiempo para InfluxDB
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Verificar conexión a InfluxDB
  if (client.validateConnection()) {
    Serial.print("Conectado a InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("Fallo la conexión a InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Prueba adicional de conexión a base de datos
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conexión a Internet establecida.");
    
    // Validar la conexión a InfluxDB
    if (client.validateConnection()) {
      Serial.println("Conexión a InfluxDB exitosa.");
    } else {
      Serial.println("Error en la conexión a InfluxDB: " + client.getLastErrorMessage());
    }
  } else {
    Serial.println("No hay conexión a Internet.");
  }
}

// Función para enviar datos a InfluxDB
void sendToInfluxDB(float temp, float hum) {
/* Esta función envía los datos de temperatura y humedad a InfluxDB. 
   Primero, se añaden los valores como campos en un objeto de datos 
   llamado 'sensor'. Luego, los datos son enviados a la base de datos 
   a través del cliente InfluxDB. Si ocurre algún error en la transmisión, 
   se imprime un mensaje de error en el monitor serial, de lo contrario, 
   confirma el éxito del envío.
*/
  sensor.clearFields();
  sensor.addField("temperature", temp);
  sensor.addField("humidity", hum);
  sensor.addField("ledRojo", redValue);
  sensor.addField("ledAzul", blueValue);
  sensor.addField("ledVerde", greenValue);

  if (!client.writePoint(sensor)) {
    Serial.print("Error al escribir en InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("Datos enviados a InfluxDB con éxito");
  }
}

// Funcion para reconectar a wifi y a influxdb
void controllerWifiInfluxdb(double temp, double hum){
/* Esta función se encarga de gestionar la reconexión a la red Wi-Fi 
   y el envío de los datos de temperatura y humedad a InfluxDB. 
   Primero, intenta establecer una conexión Wi-Fi utilizando la función 
   conectionWifi(). Luego, convierte los valores de temperatura y humedad 
   a formato float y los envía a InfluxDB mediante la función sendToInfluxDB().
*/
  // Reconectar a WiFi
  conectionWifi();
  
  // Envio de datos a InfluxDB
  float temperature_f = temp;
  float humidity_f = hum;

  sendToInfluxDB(temperature_f, humidity_f);
}

// Función para enviar información de vuelta al Nodo 1
void sendInfoToNode1() {
/* Esta función genera un mensaje en formato JSON que contiene el estado 
   actual del LED RGB (valores de rojo, verde y azul) y el número del nodo. 
   El mensaje se envía en broadcast a través de la red mesh para que otros 
   nodos, como el Nodo 1, reciban la información. Además, el mensaje 
   se imprime en el monitor serial para depuración.
*/
  JSONVar jsonResponse;

  // Información sobre el estado del LED RGB
  jsonResponse["node"] = nodeNumber; // Identificador de este nodo (Nodo 2)
  jsonResponse["ledRed"] = redValue;
  jsonResponse["ledGreen"] = greenValue;
  jsonResponse["ledBlue"] = blueValue;

  String response = JSON.stringify(jsonResponse);
  mesh.sendBroadcast(response); // Enviar el mensaje a todos los nodos

  Serial.println("Enviando estado del LED RGB al Nodo 1: ");
  Serial.println(response);
}

// Callback al recibir mensajes
void receivedCallback(uint32_t from, String &msg) {
/* Esta función es el callback que se ejecuta cuando se recibe un mensaje 
   desde otro nodo en la red mesh. Parsea el mensaje recibido en formato 
   JSON para extraer los valores de temperatura y humedad. Luego, llama a 
   la función controlLedRGB() para ajustar el LED RGB en función de la 
   temperatura, y envía la información de vuelta al Nodo 1. Después, 
   desconecta el Wi-Fi, envía los datos a InfluxDB y reinicia la red mesh.
*/
  Serial.printf("Mensaje recibido desde %u: %s\n", from, msg.c_str());

  JSONVar myObject = JSON.parse(msg.c_str());
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Error al parsear JSON");
    return;
  }

  temperature = (double)myObject["temp"];
  humidity = (double)myObject["hum"];

  controlLedRGB(temperature); // Controlar LED RGB
  sendInfoToNode1();  // Reenvio de datos
  
  mesh.stop();
  controllerWifiInfluxdb(temperature, humidity);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
}

// Funciones de MESH
void newConnectionCallback(uint32_t nodeId) {
/* Esta función es un callback que se ejecuta cuando se establece una nueva 
   conexión con otro nodo en la red mesh. Imprime el ID del nodo recién 
   conectado en el monitor serial.
*/
  Serial.printf("Nueva conexión, nodeId = %u\n", nodeId);
}
void changedConnectionCallback() {
/* Esta función se ejecuta cuando hay cambios en las conexiones de la red mesh. 
   Simplemente imprime un mensaje en el monitor serial indicando que ha habido 
   cambios en las conexiones.
*/
  Serial.println("Cambios en las conexiones");
}
void nodeTimeAdjustedCallback(int32_t offset) {
/* Esta función se ejecuta cuando el tiempo en el nodo ha sido ajustado 
   debido a la sincronización con otros nodos en la red mesh. Imprime el 
   tiempo actual y el desfase (offset) aplicado, lo que puede ser útil para 
   la sincronización temporal entre nodos.
*/
  Serial.printf("Hora ajustada %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

// Configuración inicial
void setup() {
  Serial.begin(115200);

  // Inicializar pines del LED RGB
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // Inicializar la red mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

// Bucle principal
void loop() {
  mesh.update();
}