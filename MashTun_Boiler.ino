#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"

const char* ssid     = "Set_your_WiFi_SSID";
const char* password = "Set_your_WiFi_Password";
int ssid_hidden = 0; // Is the WiFi network hidden?
const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";

int relayPin1 = 12; // Out pin relay 1st coil
int relayPin2 = 13; // Out pin relay 2nd coil
int relayPin3 = 14; // Out pin relay 3rd coil
int probe = 4; // In pin probe data

float temp = 0.0;
String tempProbe; // Temperature of the sensor (NTC sensor)
int target;

String sliderValue = "0";
const char* PARAM_INPUT = "value";

AsyncWebServer server(80);

// Setup a oneWire instance to communicate with OneWire devices
OneWire oneWire(probe);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);


String readTemperature() {
  if (isnan(temp)) {    
    Serial.println("Measurement error!");
    return "--";
  }
  else {
    return tempProbe;
  }
}



const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title> *** TITLE OF YOUR WEB PAGE ***</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h1 {font-size: 1rem;}
    h2 {font-size: 3rem; color:red;}
    p {font-size: 2rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px; background-image: url(" *** YOUR BACKGROUND IMAGE IN BASE 64 *** "); background-repeat: no-repeat; 
  background-attachment: fixed;
  background-size: cover;}
    sup {
    vertical-align: 40%;
    font-size: 60%;
    }
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .sliderP {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #00000070; border-radius: 34px}
    .sliderP:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 25px}
    input:checked+.sliderP {background-color: #b30000}
    input:checked+.sliderP:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}

    .slider { -webkit-appearance: none; margin: 10px; width: 360px; height: 40px; background: #FFF;
      outline: none;border-radius: 20px}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #b30000; cursor: pointer; border-radius: 20px}
    .slider::-moz-range-thumb { width: 35px; height: 68px; background: #b30000; cursor: pointer; border-radius: 20px}
  </style>
</head>
<body>
  <img alt="Logo Alternative Text" src=" *** YOUR LOGO IMAGE IN BASE 64 *** " />
  <h1>Panell de Control - SS Brewtech</h1>
  <h2><span id="temperature">%TEMPERATURE%</span><sup class="units">&deg;C</sup></h2>
  <p><span id="textSliderValue">%SLIDERVALUE%</span><sup class="units">&deg;C</sup></p>
  <p><input type="range" onchange="updateSliderPWM(this)" id="pwmSlider" min="0" max="100" value="%SLIDERVALUE%" step="1" class="slider"></p>
  <br>
  <p>Bomba</p>
  <label class="switch">
  <input type="checkbox" onchange="toggleCheckbox(this)" id="5" " + outputState(5) + ">
  <span class="sliderP"></span>
  </label>


  
<script>
function updateSliderPWM(element) {
  var sliderValue = document.getElementById("pwmSlider").value;
  document.getElementById("textSliderValue").innerHTML = sliderValue;
  console.log(sliderValue);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider?value="+sliderValue, true);
  xhr.send();
}

var slider = document.getElementById("pwmSlider");
var output = document.getElementById("textSliderValue");
output.innerHTML = slider.value;
slider.oninput = function() {
  output.innerHTML = this.value;
}

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 2000 ) ;

function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}

</script>
</body>
</html>
)rawliteral";

String outputState(int output){
  if(digitalRead(output)){
    return "checked";
  }
  else {
    return "";
  }
}

String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return readTemperature();
  }
  if (var == "SLIDERVALUE"){
    return sliderValue;
  }
  return String();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  sensors.begin();

  pinMode(relayPin1, OUTPUT); digitalWrite(relayPin1, 1);
  pinMode(relayPin2, OUTPUT); digitalWrite(relayPin2, 1);
  pinMode(relayPin3, OUTPUT); digitalWrite(relayPin3, 1);
  pinMode(5, OUTPUT); digitalWrite(5, 1);

  Serial.print("Starting your AP (Access Point)…");
  WiFi.softAP(ssid, password, ssid_hidden);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readTemperature().c_str());
  });

  // Send a GET request to <ESP_IP>/slider?value=<inputMessage>
  server.on("/slider", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      sliderValue = inputMessage;
      target = sliderValue.toInt();
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
  });
  
  // Start server
  server.begin();
}

void loop() {
  delay(1000);
  sensors.requestTemperatures(); 
  float temp = sensors.getTempCByIndex(0);  // Get temperature of the probe in degrees Celsius
  tempProbe = String(temp);
  Serial.print("Temp = ");
  Serial.print(temp);
  Serial.println(" ºC");
  Serial.print("Target = ");
  Serial.print(target);
  Serial.println(" ºC");
  if(temp < target){
    digitalWrite(relayPin1, 0);
    if(temp < target -1){
      digitalWrite(relayPin2, 0);
      }
      else{
        digitalWrite(relayPin2, 1);
        digitalWrite(relayPin3, 1);
        }
    if(temp < target -2){
      digitalWrite(relayPin3, 0);
      }
      else{
      digitalWrite(relayPin3, 1);
      }
    }
  else if (temp >= target){
      digitalWrite(relayPin1, 1);
      digitalWrite(relayPin2, 1);
      digitalWrite(relayPin3, 1);
      }
}
