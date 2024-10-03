#include <Adafruit_Sensor.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "DHT.h"
#include "esp_sleep.h"  // Librería para controlar el deep sleep

// MESH Details
#define MESH_PREFIX "whateveryoulike"  // Nombre para tu MESH
#define MESH_PASSWORD "somethingSneak"    // Contraseña para tu MESH
#define MESH_PORT 1515            // Puerto por defecto

#define DHTPIN 4       // Pin digital conectado al sensor DHT
#define DHTTYPE DHT11  // Tipo de sensor DHT

DHT dht(DHTPIN, DHTTYPE);

int nodeNumber = 2;  // Número de este nodo
String readings;     // String para almacenar las lecturas

Scheduler userScheduler;  // Para manejar las tareas
painlessMesh mesh;

// Prototipos de funciones
void sendMessage();
String getReadings();
void goToSleep();  // Función para entrar en deep sleep

// Crear tareas para enviar mensajes y obtener lecturas
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);  

String getReadings() {
/* Esta función recoge los datos del sensor DHT11, específicamente la
   temperatura y la humedad. Luego, estos valores se almacenan en un 
   objeto JSON, junto con el número del nodo. El objeto JSON se convierte
   a una cadena (string) para ser enviada a otros nodos. Los valores también 
   se imprimen en el monitor serial para facilitar la depuración.
*/
  JSONVar jsonReadings;

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.println(F("°C "));

  jsonReadings["node"] = nodeNumber;
  jsonReadings["temp"] = temperature;
  jsonReadings["hum"] = humidity;
  
  readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage() {
/* Esta función obtiene los datos del sensor llamando a getReadings() y 
   envía el mensaje resultante a través de la red mesh utilizando broadcast.
   El mensaje también se imprime en el monitor serial para su verificación.
   Después de enviar el mensaje, se puede activar la función de deep sleep
   para ahorrar energía.
*/
  String msg = getReadings();
  Serial.println(msg);
  mesh.sendBroadcast(msg);
  
  // Después de enviar el mensaje, entra en deep sleep
  //goToSleep();
}

void goToSleep() {
/* Esta función pone al dispositivo ESP32 en modo deep sleep durante 10 
   segundos. Configura un temporizador que despierta al microcontrolador
   después de ese tiempo, para ahorrar energía entre las lecturas de datos
   del sensor y el envío de mensajes.
*/
  Serial.println("Entrando en Deep Sleep durante 10 segundos...");
  
  // Configurar el temporizador para despertar después de 10 segundos
  esp_sleep_enable_timer_wakeup(10 * 1000000);  // 10 segundos (en microsegundos)
  
  // Poner al ESP32 en modo deep sleep
  esp_deep_sleep_start();
}

// Callbacks necesarios para la librería painlessMesh
void receivedCallback(uint32_t from, String &msg) {
/* Esta función se llama automáticamente cuando se recibe un mensaje en
   la red mesh. Imprime el ID del nodo de origen y el mensaje recibido
   en el monitor serial para depuración.
*/
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
/* Esta función se invoca cuando se establece una nueva conexión en la
   red mesh. Imprime el ID del nuevo nodo conectado en el monitor serial.
*/
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
/* Esta función se llama cuando cambia alguna conexión en la red mesh,
   ya sea porque se añade o se pierde un nodo. Solo imprime un mensaje 
   indicando que las conexiones han cambiado.
*/
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
/* Esta función se invoca cuando el tiempo de la red mesh se ajusta.
   Imprime el tiempo actual del nodo y el desfase (offset) aplicado, 
   lo cual puede ser útil para sincronización entre nodos.
*/
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  Serial.begin(115200);
  
  // Iniciar el sensor DHT
  dht.begin();

  // Configurar la malla mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  // Agregar la tarea de enviar mensaje
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();  // Habilitar la tarea para que se ejecute una vez
}

void loop() {
  // Actualizar la malla
  mesh.update();
}
