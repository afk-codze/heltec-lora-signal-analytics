#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define QUEUE_LENGTH 4096
#define TEST_DURATION_MS 1000

QueueHandle_t testQueue;
volatile bool testRunning = false;

// Fill the queue continuously during the test
void producerTask(void *pvParameters) {
  int dummyData = 42;
  while (true) {
    if (testRunning) {
      xQueueSend(testQueue, &dummyData, 0); // non-blocking send
    } else {
      vTaskDelay(pdMS_TO_TICKS(10)); // idle when not testing
    }
  }
}

// Read from queue for exactly 1 second
void consumerTask(void *pvParameters) {
  vTaskDelay(pdMS_TO_TICKS(1000)); // wait for system to stabilize

  int receivedData;
  uint32_t count = 0;
  uint32_t startTime = millis();

  testRunning = true;

  while ((millis() - startTime) < TEST_DURATION_MS) {
    if (xQueueReceive(testQueue, &receivedData, 0) == pdPASS) {
      count++;
    }
  }

  testRunning = false;

  Serial.print("Items read in ");
  Serial.print(TEST_DURATION_MS);
  Serial.print(" ms: ");
  Serial.println(count);
  Serial.print("Read rate: ");
  Serial.print((float)count / (TEST_DURATION_MS / 1000.0));
  Serial.println(" Hz");

  vTaskDelete(NULL); // Done
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting realistic queue read rate test...");

  testQueue = xQueueCreate(QUEUE_LENGTH, sizeof(int));
  if (testQueue == NULL) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  xTaskCreatePinnedToCore(producerTask, "Producer", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(consumerTask, "Consumer", 2048, NULL, 1, NULL, 1);
}

void loop() {
  // Nothing here
}
