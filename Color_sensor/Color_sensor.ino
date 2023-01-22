 #include "src/WifiConnection.h"
 #include "src/FirebaseDb.h"
 #include "src/Application.h"

 #define MOTOR_RELAY_OUTPUT 14

void setMotor(bool state)
{
  static bool motor_state = false;
  if (motor_state == state) return;
  
  digitalWrite(MOTOR_RELAY_OUTPUT, (state ? HIGH : LOW));
  motor_state = state;
}

void Wifi_failureHandler(void)
{
  Serial.println("Wifi failure handled");
  setMotor(false); // Shutdown motor
  Database_refreshConnection();
}

void App_objectDetectionHandler(t_Color * color)
{
  (void)Database_pushObjectColor(color);
}

void App_objectOverheightHandler(void)
{
  Serial.println("Object overheight handled");
  setMotor(false); // Shutdown motor
}

void setup()
{
  Serial.begin(115200);

  pinMode(MOTOR_RELAY_OUTPUT, OUTPUT);
  digitalWrite(MOTOR_RELAY_OUTPUT, LOW);
  
  App_init();
  App_setObjectDetectionCallback(App_objectDetectionHandler);
  App_setObjectOverheightCallback(App_objectOverheightHandler);
  
  Wifi_init();
  Wifi_setFailureCallback(Wifi_failureHandler);
}

void loop()
{
  // Constantly monitor wifi connection
  Wifi_task();

  // Monitor connection to Firebase database
  if (Wifi_getState() == WIFI_CONNECTED)
  {
    Database_task();
  }
  else
  {
    delay(200);
    return;
  }

  // Application task (object detection)
  if (Database_getState() == FIREBASE_CONNECTED)
  {
    App_task();

    if (!App_getOverheightCondition())
    {
      setMotor(true); // Run motor
    }
  }
}
