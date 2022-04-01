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
String host = "https://192.168.10.245:6969/api/User_/user/game/";
uint8_t port = 6969;

bool received_game_data = false;
bool buttons_enabled = false;

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

    detachInterrupt(light_red);
    detachInterrupt(light_yellow);
    detachInterrupt(light_green);

    // -------------- CONNECT TO WIFI --------------

    Serial.println("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    //check wi-fi is connected to wi-fi network
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(1000);
        Serial.print(".");
    }

    client.setInsecure();

    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

    // ----------------------------------------------

    Serial.println("Waiting for App to start game...");
}

void enable_buttons()
{
    // interrupt modes are set to FALLING cause I had issues with RISING, it would sometimes trigger the interrupt twice on 1 button press
    attachInterrupt(digitalPinToInterrupt(button_red), on_button_red_click, FALLING);
    attachInterrupt(digitalPinToInterrupt(button_yellow), on_button_yellow_click, FALLING);
    attachInterrupt(digitalPinToInterrupt(button_green), on_button_green_click, FALLING);
    buttons_enabled = true;
}

void disable_buttons()
{
    detachInterrupt(light_red);
    detachInterrupt(light_yellow);
    detachInterrupt(light_green);
    buttons_enabled = false;
}

void send_result_to_api(int id, int score)
{
    Serial.print("user_id: ");
    Serial.println(id);
    Serial.print("score: ");
    Serial.println(score);

    String str_result_path = "result?id=" + String(id) + "&score=" + String(score);

    String full_path = host + str_result_path;

    Serial.print("full path: ");
    Serial.println(full_path);

    client.connect(full_path, port);
    http.begin(client, full_path);
    delay(100);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Content-Length", String(full_path.length()));
    int result_response = http.POST(""); // yes, this is very interesting

    String payload = http.getString();

    Serial.print("Result has been sent with response code: ");
    Serial.println(result_response);
    Serial.print("payload for response: ");
    Serial.println(payload);

    http.end();
    user_inputs.clear();

    received_game_data = false; // ready for the next game
    Serial.println("Waiting for App to start game...");
}

void calculate_result(int user_id) 
{
    Serial.println("Calculating result...");
    int score = 0;

    for (int i = 0; i < lights_output.size(); i += 1)
    {
        if (lights_output[i] == user_inputs[i]) 
        {
            score += 1;
        }
    }

    send_result_to_api(user_id, score);
}

void play_game(String game_data, String game_user_id)
{
    int user_id = atoi(game_user_id.c_str()); // convert string to int

    // remove all the unneccesary chars from the string
    game_data.replace("[", "");
    game_data.replace("]", "");
    game_data.replace(",", "");
    game_data.replace(" ", "");

    Serial.println("Remember the light pattern!");
    delay(5000);

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

    Serial.println("Press the buttons in the correct order!");
    int buttons_left_start = counter;
    int buttons_left = buttons_left_start;

    Serial.print("Amount of light sequences: ");
    Serial.println(counter);

    Serial.print("Buttons left to press: ");
    Serial.println(buttons_left);

    enable_buttons();

    while (user_inputs.size() < counter) // wait for user inputs
    {
        if (buttons_left_start - user_inputs.size() < buttons_left) 
        {
            buttons_left -= 1;
            Serial.print("Buttons left to press: ");
            Serial.println(buttons_left);
        }

        delay(100);
    }

    disable_buttons();

    calculate_result(user_id);
}

/// <summary>
/// this loop sends a get request to the API every 5 seconds
/// if the first get request succeeds (which contains the puzzle itself (I should've just made the puzzle in the NodeMCU instead of sending it via an API request))
/// the second get request will get the user_id of the user that wants to play
/// if either request fails, it will start over
/// </summary>

void loop()
{
    if (buttons_enabled)
    {
        disable_buttons();
    }

    while(!received_game_data)
    {
        http.begin(client, path + "/user/game/game_data");
        int game_data_response = http.GET();

        if (game_data_response > 0)
        {
            String game_data_payload = http.getString();

            if (game_data_payload.length() > 2) // an "empty" get request would be [], want the payload.length() to be over 2 just to be sure
            {
                received_game_data = true;
                http.end();

                http.begin(client, path + "/user/game/game_user_id");
                int game_user_id_response = http.GET();

                if (game_user_id_response > 0)
                {
                    String game_user_id_payload = http.getString();
                    http.end();

                    Serial.println("Playing game");

                    play_game(game_data_payload, game_user_id_payload); // journey: play_game() --> calculate_result() --> send_result_to_api()
                }
                else 
                {
                    Serial.println("----- ERROR -----");
                    Serial.println(game_user_id_response);
                    Serial.println("-----------------");
                }
                http.end();
            }
        }
        else
        {
            Serial.println("----- ERROR -----");
            Serial.println(game_data_response);
            Serial.println("-----------------");
        }
        http.end();

        delay(5000); // call http.get every 5 seconds
    }
}