#include <WiFi.h>
#include <WebServer.h>

#define trigPin 12   // Pin Trig sensor ultrasonik
#define echoPin 13   // Pin Echo sensor ultrasonik
#define in1 18      // Pin IN1 motor driver L298N
#define in2 19      // Pin IN2 motor driver L298N
#define enA 21      // Pin ENA untuk mengontrol kecepatan motor

const char* ssid = "mie ayam";        // Ganti dengan nama WiFi Anda
const char* password = "bungkuspedes"; // Ganti dengan password WiFi Anda

WebServer server(80);  // Membuat server pada port 80

long duration;
float distance;
float targetDistance = 21.5; // Default target, bisa diubah sesuai kebutuhan
bool liftMoving = false;

int pwmChannel = 0;
int freq = 5000;
int resolution = 8;

enum LiftStatus {
  IDLE,
  MOVING_UP,
  MOVING_DOWN
};

LiftStatus liftStatus = IDLE;

unsigned long lastMeasureTime = 0; // Waktu terakhir pengukuran
const unsigned long measureInterval = 50; // Interval pengukuran (ms)

void setup() {
  Serial.begin(115200);
  
  // Setup motor dan sensor ultrasonik
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  ledcSetup(pwmChannel, freq, resolution); // Setup PWM
  ledcAttachPin(enA, pwmChannel); // Attach PWM to pin enA
  
  // Setup WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Setup halaman web server
  server.on("/", handleRoot);
  server.on("/lantai1", [](){ handleLift(21.5); });
  server.on("/lantai2", [](){ handleLift(11.8); });
  server.on("/lantai3", [](){ handleLift(2.4); });
  
  server.begin();  // Memulai server web
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();  // Menangani permintaan dari web client
  float measureCM = measureDistance();
  
  // Jalankan pengukuran jarak ultrasonik secara terus-menerus
  if (millis() - lastMeasureTime >= measureInterval) {
    distance = measureCM;
    lastMeasureTime = millis(); // Update waktu terakhir pengukuran
  }
  
  // Jika lift sedang bergerak, cek apakah lift telah mencapai lantai yang dituju
  if (liftMoving) {
    moveLift(distance, targetDistance);
  }
}

// Fungsi untuk mengukur jarak menggunakan sensor ultrasonik
float measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(1);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH);
  float distance = (duration * 0.034) / 2; // Menghitung jarak dalam cm
  Serial.print("Jarak : ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

float tolerance = 0.22;  // Toleransi jarak dalam cm

// Fungsi untuk menggerakkan lift
void moveLift(float currentDistance, float targetDistance) {
  Serial.print("Current Distance: ");
  Serial.print(currentDistance);
  Serial.print(" Target Distance: ");
  Serial.println(targetDistance);

  float distanceDifference = abs(currentDistance - targetDistance);

  // Tentukan status lift berdasarkan jarak
  if (distanceDifference <= tolerance) {
    // Jika jarak sudah berada dalam toleransi, hentikan lift
    liftStatus = IDLE;
    stopLift();  // Hentikan lift jika dalam toleransi
  } else if (currentDistance > targetDistance) {
    // Jika lift harus turun
    liftStatus = MOVING_DOWN;
  } else if (currentDistance < targetDistance) {
    // Jika lift harus naik
    liftStatus = MOVING_UP;
  }

  // Tindakan berdasarkan status lift
  switch (liftStatus) {
    case MOVING_UP:
      movingUp(distanceDifference);
      break;
    case MOVING_DOWN:
      movingDown(distanceDifference);
      break;
    case IDLE:
      stopLift();
      break;
  }
}

// Fungsi untuk menggerakkan lift ke atas
void movingUp(float distanceDifference) {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  
  // Kurangi kecepatan saat mendekati target
  if (distanceDifference < 2.1) {  // Jika jarak kurang dari 5 cm
    ledcWrite(pwmChannel, 180);  // Kecepatan setengah
  } else {
    ledcWrite(pwmChannel, 196);  // Kecepatan penuh
  }
  Serial.println("Lift naik ke lantai target...");
}

// Fungsi untuk menggerakkan lift ke bawah
void movingDown(float distanceDifference) {
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  
  // Kurangi kecepatan saat mendekati target
  if (distanceDifference < 1.7) {  // Jika jarak kurang dari 5 cm
    ledcWrite(pwmChannel, 178);  // Kecepatan setengah
  } else {
    ledcWrite(pwmChannel, 190);  // Kecepatan penuh
  }
  Serial.println("Lift turun ke lantai target...");
}

// Fungsi untuk menghentikan lift
void stopLift() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  ledcWrite(pwmChannel, 0);  // Matikan motor
  liftMoving = false;        // Hentikan gerakan motor
  Serial.println("Lift berhenti di lantai target.");
}

// Fungsi untuk mengatur target sesuai lantai yang diinginkan
void setTarget(float target) {
  targetDistance = target;
  liftMoving = true;
  Serial.print("Set Target Distance: ");
  Serial.println(target);
}

// Fungsi untuk menangani tombol pada web dan mengatur lantai
void handleLift(float target) {
  setTarget(target);
  server.send(200, "text/html", "<html><body><h1>Lift sedang bergerak ke target...</h1></body></html>");
}

// Halaman web utama
void handleRoot() {
  String html = "<html>\
    <head>\
      <title>Lift Control</title>\
      <style>\
        body {\
          font-family: Arial, sans-serif;\
          margin: 0;\
          padding: 0;\
          text-align: center;\
        }\
        h1 {\
          margin: 20px 0;\
        }\
        p {\
          margin: 0;\
        }\
        button {\
          width: 90%;\
          height: 20%;\
          padding: 15px;\
          font-size: 65px;\
          border: none;\
          color: white;\
          background-color: #00cc67;\
          cursor: pointer;\
          margin: 5px 0;\
          border-radius: 5px;\
        }\
        button:hover {\
          background-color: #0056b3;\
        }\
        @media (max-width: 600px) {\
          button {\
            font-size: 20px;\
          }\
        }\
      </style>\
    </head>\
    <body>\
      <h1>Kontrol Lift</h1>\
      <p><a href=\"/lantai1\"><button>Lantai 1</button></a></p>\
      <p><a href=\"/lantai2\"><button>Lantai 2</button></a></p>\
      <p><a href=\"/lantai3\"><button>Lantai 3</button></a></p>\
    </body>\
  </html>";
  server.send(200, "text/html", html);
}
