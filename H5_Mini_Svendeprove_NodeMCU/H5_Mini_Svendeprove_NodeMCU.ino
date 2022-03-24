#ifdef VM_DEBUG_GDB    
#include "GDBStub.h"
#endif

#include <ESP8266WiFi.h> // header for wifi
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // http client
#include <algorithm>

WiFiClientSecure client;
HTTPClient http;

const char* ssid = "NodeMCU_Station_Mode";
const char* password = "h5pd091121_Styrer";

String path = "https://192.168.10.245:6969/api/User_";

bool received_game_data = false;
std::vector<int> user_inputs;
std::vector<int> lights_output;

// -------------- PINS --------------

uint8_t button_red = D1;
uint8_t button_yellow = D2;
uint8_t button_green = D3;

uint8_t light_red = D5;
uint8_t light_yellow = D6;
uint8_t light_green = D7;

// ----------------------------------

// -------------- BUTTON INTERRUPTS --------------

IRAM_ATTR void on_button_red_click()
{
    user_inputs.push_back(1);
}

IRAM_ATTR void on_button_yellow_click()
{
    user_inputs.push_back(2);
}

IRAM_ATTR void on_button_green_click()
{
    user_inputs.push_back(3);
}

// -----------------------------------------------

void change_state(uint8_t light) 
{
    digitalWrite(light, !(digitalRead(light)));  //Invert Current State of LED
}

// the setup function runs once when you press reset or power the board
void setup() 
{

#ifdef VM_DEBUG_GDB    gdbstub_init();
    // Add/extend the below delay if you want to debug the setup code
    // ENABLING HARDWARE DEBUGGING JUST RETURNS "STRING IS NULL OR EMPTY"
    delay(3000);
#endif

    Serial.begin(115200);
    delay(100);

    pinMode(button_red, INPUT_PULLUP);
    pinMode(button_yellow, INPUT_PULLUP);
    pinMode(button_green, INPUT_PULLUP);

    pinMode(light_red, OUTPUT);
    pinMode(light_yellow, OUTPUT);
    pinMode(light_green, OUTPUT);

    state1 = light_off;

    // -------------- CONNECT TO WIFI --------------

    client.setInsecure();

    Serial.println("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    //check wi-fi is connected to wi-fi network
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(1000);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

    // ----------------------------------------------
}

void send_result_to_api(int score)
{

}

void calculate_result() 
{
    int score = 0;

    for (int i = 0; i < lights_output.size(); i += 1)
    {
        if (lights_output[i] == user_inputs[i]) 
        {
            score += 1;
        }
    }

    send_result_to_api(score);
}

void play_game(String game_data)
{
    detachInterrupt(light_red);
    detachInterrupt(light_yellow);
    detachInterrupt(light_green);

    // remove all the unneccesary chars from the string
    game_data.replace("[", "");
    game_data.replace("]", "");
    game_data.replace(",", "");
    game_data.replace(" ", "");

    int light = 1;
    int counter = 0;

    for (int i = 1; i < game_data.length() + 1; i += 1)
    {
        if (game_data[i - 1] == '1') // if the character at this position is a '1' character
        {
            lights_output.push_back(light); // add the light number to a list for score comparison later

            switch (light)
            {
            case 1:
                change_state(light_red);
                delay(500);
                change_state(light_red);
                break;

            case 2:
                change_state(light_yellow);
                delay(500);
                change_state(light_yellow);
                break;

            case 3:
                change_state(light_green);
                delay(500);
                change_state(light_green);
                break;
            }
        }

        light += 1; // go to the next light, order is 1 = red, 2 = yellow, 3 = green

        if (i % 3 == 0)
        {
            delay(500);
            light = 1;
            counter += 1; // counts the total amount of lights that lit up
        }
    }

    // interrupt modes are set to FALLING cause I had issues with RISING, it would sometimes trigger the interrupt twice on 1 button press
    attachInterrupt(digitalPinToInterrupt(button_red), on_button_red_click, FALLING);
    attachInterrupt(digitalPinToInterrupt(button_yellow), on_button_yellow_click, FALLING);
    attachInterrupt(digitalPinToInterrupt(button_green), on_button_green_click, FALLING);
    
    while (user_inputs.size() < counter) // wait for user inputs
    {
        delay(100);
    }

    calculate_result();
}

void loop()
{
    while(!received_game_data)
    {
        http.begin(client, path + "/user/game/game_data");
        int response = http.GET();

        if (response > 0)
        {
            String payload = http.getString();

            if (payload.length() > 2) // an "empty" get request would be [], want the payload.length() to be over 2 just to be sure
            {
                received_game_data = true;
                http.end();
                play_game(payload); // journey: play_game() --> calculate_result() --> send_result_to_api()
                break;
            }
        }
        else
        {
            Serial.println("----- ERROR -----");
            Serial.println(response);
            Serial.println("-----------------");
        }
        http.end();
        delay(5000); // call http.get every 5 seconds
    }
}