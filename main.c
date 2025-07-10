#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include <ESP32Servo.h>

// --- CONFIGURAÇÕES DE REDE ---
const char *ssid = "Rede Oculta";  // <-- NOME DA SUA REDE AQUI
const char *password = "20145600"; // <-- SENHA DA SUA REDE AQUI

// --- DEFINIÇÃO DOS PINOS ---
#define LED1_VERDE_PIN 23
#define LED1_VERMELHO_PIN 4
#define LED2_VERDE_PIN 21
#define LED2_VERMELHO_PIN 5
#define BUZZER_PIN 22
#define SERVO1_PIN 18
#define SERVO2_PIN 19
#define RXD2 16
#define TXD2 17

// --- OBJETOS E VARIÁVEIS GLOBAIS ---
WebServer server(80);
Preferences preferences;
Servo servo1;
Servo servo2;
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

struct UserData
{
    char name[50];
    char cpf[20];
    char matricula[20];
    int drawer;
};

// --- VARIÁVEIS DE ESTADO PARA O CADASTRO WEB ---
enum EnrollmentState
{
    IDLE,
    WAITING_FOR_FIRST_IMAGE,
    REMOVE_FINGER,
    WAITING_FOR_SECOND_IMAGE,
    ENROLL_SUCCESS,
    ENROLL_FAILURE,
    ENROLL_ABORTED
};
volatile EnrollmentState enrollmentState = IDLE;
UserData pending_user;
int pending_id = 0;
String lastStatusMessage = "Sistema pronto.";

// --- PROTÓTIPOS DE FUNÇÕES ---
String getHtmlHeader(String title);
String getHtmlFooter();
void handleRoot();
void handleList();
void handleRegisterForm();
void handleDoRegisterAJAX();
void handleEnrollmentStatus();
void handleDelete();
void handleNotFound();
void handleCSS();

void initializeHardware();
void openDrawer(int drawerNumber, String userName);
void enrollStateMachine();
void resetEnrollment();

// --- SETUP ---
void setup()
{
    Serial.begin(115200);
    initializeHardware();

    Serial.print("\nConectando ao WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Conectado!");
    Serial.print("IP Address: http://");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/list", HTTP_GET, handleList);
    server.on("/register", HTTP_GET, handleRegisterForm);
    server.on("/doregister", HTTP_POST, handleDoRegisterAJAX);
    server.on("/enroll-status", HTTP_GET, handleEnrollmentStatus);
    server.on("/delete", HTTP_GET, handleDelete);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("Servidor web iniciado.");
}

// --- LOOP ---
void loop()
{
    server.handleClient();

    // Executa a máquina de estados de cadastro se estiver ativa
    if (enrollmentState != IDLE)
    {
        enrollStateMachine();
    }
    else
    {
        // Operação normal: verifica digital para abrir gaveta
        int foundID = getFingerprintIDez();
        if (foundID != -1)
        {
            UserData user = loadUserData(foundID);
            openDrawer(user.drawer, String(user.name));
        }
    }
    delay(50); // Pequeno delay para estabilidade
}

// =======================================================
//  MÁQUINA DE ESTADOS DE CADASTRO (NÃO-BLOQUEANTE)
// =======================================================
void enrollStateMachine()
{
    uint8_t p = -1;

    switch (enrollmentState)
    {
    case WAITING_FOR_FIRST_IMAGE:
        p = finger.getImage();
        if (p == FINGERPRINT_OK)
        {
            p = finger.image2Tz(1);
            if (p == FINGERPRINT_OK)
            {
                enrollmentState = REMOVE_FINGER;
            }
            else
            {
                enrollmentState = ENROLL_FAILURE;
            }
        }
        break;

    case REMOVE_FINGER:
        p = finger.getImage();
        if (p == FINGERPRINT_NOFINGER)
        {
            enrollmentState = WAITING_FOR_SECOND_IMAGE;
        }
        break;

    case WAITING_FOR_SECOND_IMAGE:
        p = finger.getImage();
        if (p == FINGERPRINT_OK)
        {
            p = finger.image2Tz(2);
            if (p == FINGERPRINT_OK)
            {
                p = finger.createModel();
                if (p == FINGERPRINT_OK)
                {
                    p = finger.storeModel(pending_id);
                    if (p == FINGERPRINT_OK)
                    {
                        saveUserData(pending_id, pending_user);
                        lastStatusMessage = "Usuário " + String(pending_user.name) + " cadastrado com sucesso!";
                        enrollmentState = ENROLL_SUCCESS;
                    }
                    else
                    {
                        enrollmentState = ENROLL_FAILURE;
                    }
                }
                else
                {
                    enrollmentState = ENROLL_FAILURE;
                }
            }
            else
            {
                enrollmentState = ENROLL_FAILURE;
            }
        }
        break;

    case ENROLL_SUCCESS:
    case ENROLL_FAILURE:
    case ENROLL_ABORTED:
        // Estados finais, aguarda o JS buscar o resultado e reseta.
        break;

    default:
        enrollmentState = IDLE;
        break;
    }
}

// =======================================================
//  HANDLERS DA INTERFACE WEB
// =======================================================

String getHtmlHeader(String title)
{
    String header = "<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>";
    header += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    header += "<title>" + title + " - Controle de Gavetas</title>";
    header += "<style>";
    header += "body{background-color:#1a1a1a;color:#e0e0e0;font-family:Arial,sans-serif;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center;}";
    header += ".container{background-color:#2c2c2c;border-radius:8px;padding:20px 40px;width:100%;max-width:800px;box-shadow:0 4px 8px rgba(0,0,0,0.3);}";
    header += "h1,h2{color:#00bcd4;border-bottom:2px solid #00bcd4;padding-bottom:10px;}";
    header += "a.button, button{background-color:#00bcd4;color:#1a1a1a;text-decoration:none;padding:12px 20px;border-radius:5px;border:none;cursor:pointer;font-size:16px;font-weight:bold;transition:background-color 0.3s,transform 0.2s;}";
    header += "a.button:hover, button:hover{background-color:#0097a7;transform:scale(1.05);}";
    header += "a.button.danger{background-color:#d9534f;} a.button.danger:hover{background-color:#c9302c;}";
    header += ".nav{margin-bottom:20px;display:flex;gap:10px;}";
    header += "table{width:100%;border-collapse:collapse;margin-top:20px;} th,td{padding:12px;text-align:left;border-bottom:1px solid #444;}";
    header += "th{background-color:#333;color:#00bcd4;} tr:nth-child(even){background-color:#333;}";
    header += "form label{display:block;margin-top:15px;font-weight:bold;} input,select{width:calc(100% - 20px);padding:10px;margin-top:5px;border-radius:5px;border:1px solid #555;background-color:#333;color:#e0e0e0;font-size:16px;}";
    header += "#status-box{margin-top:20px;padding:15px;border-radius:5px;font-size:18px;text-align:center;border:1px solid #555;min-height:50px;display:flex;align-items:center;justify-content:center;}";
    header += ".status-wait{border-color:#f0ad4e;color:#f0ad4e;} .status-ok{border-color:#5cb85c;color:#5cb85c;} .status-error{border-color:#d9534f;color:#d9534f;}";
    header += ".delete-icon{width:16px;height:16px;vertical-align:middle;margin-left:5px;}";
    header += "</style></head><body><div class='container'><h1>" + title + "</h1>";
    return header;
}

String getHtmlFooter()
{
    return "</div></body></html>";
}

void handleRoot()
{
    String html = getHtmlHeader("Painel de Controle");
    html += "<h2>Bem-vindo!</h2>";
    html += "<div id='status-box'>Último Status: " + lastStatusMessage + "</div>";
    html += "<h2>Ações</h2>";
    html += "<div class='nav'>";
    html += "<a href='/register' class='button'>Novo Cadastro</a>";
    html += "<a href='/list' class='button'>Listar Usuários</a>";
    html += "</div>";
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleList()
{
    String html = getHtmlHeader("Usuários Cadastrados");
    html += "<div class='nav'><a href='/' class='button'>Voltar ao Painel</a></div>";
    html += "<table><tr><th>ID</th><th>Nome</th><th>CPF</th><th>Matrícula</th><th>Gaveta</th><th>Ação</th></tr>";

    finger.getTemplateCount();
    if (finger.templateCount == 0)
    {
        html += "<tr><td colspan='6' style='text-align:center;'>Nenhum usuário cadastrado.</td></tr>";
    }
    else
    {
        for (int i = 1; i <= 127; i++)
        {
            if (finger.loadModel(i) == FINGERPRINT_OK)
            {
                UserData user = loadUserData(i);
                html += "<tr>";
                html += "<td>" + String(i) + "</td>";
                html += "<td>" + String(user.name) + "</td>";
                html += "<td>" + String(user.cpf) + "</td>";
                html += "<td>" + String(user.matricula) + "</td>";
                html += "<td>" + String(user.drawer) + "</td>";
                html += "<td><a href='/delete?id=" + String(i) + "' class='button danger' onclick='return confirm(\"Tem certeza que deseja remover " + String(user.name) + "?\")'>";
                html += "Remover <svg class='delete-icon' viewBox='0 0 24 24' fill='currentColor'><path d='M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z'/></svg></a></td>";
                html += "</tr>";
            }
        }
    }
    html += "</table>";
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleRegisterForm()
{
    resetEnrollment(); // Garante que qualquer cadastro anterior foi limpo
    String html = getHtmlHeader("Cadastrar Novo Usuário");
    html += "<div class='nav'><a href='/' class='button'>Cancelar</a></div>";
    html += "<form id='register-form'>";
    html += "<label for='name'>Nome Completo:</label><input type='text' id='name' name='name' required>";
    html += "<label for='cpf'>CPF:</label><input type='text' id='cpf' name='cpf' required>";
    html += "<label for='matricula'>Matrícula:</label><input type='text' id='matricula' name='matricula' required>";
    html += "<label for='drawer'>Gaveta:</label><select id='drawer' name='drawer'><option value='1'>Gaveta 1</option><option value='2'>Gaveta 2</option></select>";
    html += "<br><br><button type='submit'>Iniciar Cadastro da Digital</button>";
    html += "</form>";
    html += "<h2>Status do Cadastro</h2>";
    html += "<div id='status-box'>Aguardando início...</div>";

    // --- JAVASCRIPT ---
    html += "<script>";
    html += "const form = document.getElementById('register-form');";
    html += "const statusBox = document.getElementById('status-box');";
    html += "let pollingInterval;";
    html += "form.addEventListener('submit', async (e) => {";
    html += "e.preventDefault();";
    html += "clearInterval(pollingInterval);";
    html += "const formData = new FormData(form);";
    html += "const response = await fetch('/doregister', { method: 'POST', body: new URLSearchParams(formData) });";
    html += "const result = await response.json();";
    html += "if (result.success) {";
    html += "statusBox.innerHTML = 'Formulário aceito. Iniciando sensor...';";
    html += "statusBox.className = 'status-box status-wait';";
    html += "pollingInterval = setInterval(getEnrollmentStatus, 1000);"; // Inicia a verificação
    html += "} else {";
    html += "statusBox.innerHTML = 'Erro: ' + result.message;";
    html += "statusBox.className = 'status-box status-error';";
    html += "}});";
    html += "async function getEnrollmentStatus() {";
    html += "const response = await fetch('/enroll-status');";
    html += "const status = await response.json();";
    html += "statusBox.innerHTML = status.message;";
    html += "if (status.state === 'ENROLL_SUCCESS') {";
    html += "clearInterval(pollingInterval);";
    html += "statusBox.className = 'status-box status-ok';";
    html += "setTimeout(() => { window.location.href = '/list'; }, 2000);";
    html += "} else if (status.state === 'ENROLL_FAILURE' || status.state === 'ENROLL_ABORTED') {";
    html += "clearInterval(pollingInterval);";
    html += "statusBox.className = 'status-box status-error';";
    html += "} else {";
    html += "statusBox.className = 'status-box status-wait';";
    html += "}}";
    html += "</script>";

    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleDoRegisterAJAX()
{
    if (enrollmentState != IDLE)
    {
        server.send(409, "application/json", "{\"success\":false, \"message\":\"Sistema ocupado com outro cadastro.\"}");
        return;
    }

    // Verifica duplicidade
    for (int i = 1; i <= 127; i++)
    {
        if (finger.loadModel(i) == FINGERPRINT_OK)
        {
            UserData existingUser = loadUserData(i);
            if (strcmp(existingUser.cpf, server.arg("cpf").c_str()) == 0 || strcmp(existingUser.matricula, server.arg("matricula").c_str()) == 0)
            {
                server.send(409, "application/json", "{\"success\":false, \"message\":\"CPF ou Matrícula já cadastrado.\"}");
                return;
            }
        }
    }

    pending_id = getFreeID();
    if (pending_id == -1)
    {
        server.send(507, "application/json", "{\"success\":false, \"message\":\"Memória do sensor está cheia.\"}");
        return;
    }

    // Armazena dados temporariamente
    strncpy(pending_user.name, server.arg("name").c_str(), sizeof(pending_user.name) - 1);
    strncpy(pending_user.cpf, server.arg("cpf").c_str(), sizeof(pending_user.cpf) - 1);
    strncpy(pending_user.matricula, server.arg("matricula").c_str(), sizeof(pending_user.matricula) - 1);
    pending_user.drawer = server.arg("drawer").toInt();

    enrollmentState = WAITING_FOR_FIRST_IMAGE; // Inicia a máquina de estados

    server.send(200, "application/json", "{\"success\":true}");
}

void handleEnrollmentStatus()
{
    String message = "";
    String stateStr = "IDLE";

    switch (enrollmentState)
    {
    case WAITING_FOR_FIRST_IMAGE:
        message = "Aguardando 1º toque... Por favor, coloque o dedo no sensor.";
        stateStr = "WAITING";
        break;
    case REMOVE_FINGER:
        message = "OK! Agora, retire o dedo do sensor.";
        stateStr = "WAITING";
        break;
    case WAITING_FOR_SECOND_IMAGE:
        message = "Aguardando 2º toque... Coloque o mesmo dedo novamente.";
        stateStr = "WAITING";
        break;
    case ENROLL_SUCCESS:
        message = "Digital cadastrada com sucesso! Redirecionando...";
        stateStr = "ENROLL_SUCCESS";
        break;
    case ENROLL_FAILURE:
        message = "Falha no cadastro. Tente novamente.";
        stateStr = "ENROLL_FAILURE";
        break;
    case ENROLL_ABORTED:
        message = "Cadastro cancelado.";
        stateStr = "ENROLL_ABORTED";
        break;
    default:
        message = "Aguardando início...";
        stateStr = "IDLE";
        break;
    }

    String json = "{\"state\":\"" + stateStr + "\", \"message\":\"" + message + "\"}";
    server.send(200, "application/json", json);

    // Se o cadastro terminou (sucesso ou falha), reseta para o próximo
    if (enrollmentState == ENROLL_SUCCESS || enrollmentState == ENROLL_FAILURE)
    {
        delay(1000); // Dá tempo para o JS buscar o status final
        resetEnrollment();
    }
}

void handleDelete()
{
    int id = server.arg("id").toInt();
    if (id > 0)
    {
        if (finger.deleteModel(id) == FINGERPRINT_OK)
        {
            char key[10];
            sprintf(key, "u%d", id);
            preferences.remove(key);
            lastStatusMessage = "Usuário com ID " + String(id) + " foi removido.";
        }
        else
        {
            lastStatusMessage = "Falha ao remover ID " + String(id) + ".";
        }
    }
    // Redireciona de volta para a lista
    server.sendHeader("Location", "/list", true);
    server.send(302, "text/plain", "");
}

void handleNotFound()
{
    String html = getHtmlHeader("Erro 404");
    html += "<h2>Página Não Encontrada</h2>";
    html += "<p>O recurso que você procurou não foi encontrado.</p>";
    html += "<a href='/' class='button'>Voltar ao Início</a>";
    html += getHtmlFooter();
    server.send(404, "text/html", html);
}

// =======================================================
//  FUNÇÕES DE HARDWARE E LÓGICA
// =======================================================

void initializeHardware()
{
    pinMode(LED1_VERDE_PIN, OUTPUT);
    pinMode(LED1_VERMELHO_PIN, OUTPUT);
    pinMode(LED2_VERDE_PIN, OUTPUT);
    pinMode(LED2_VERMELHO_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    servo1.attach(SERVO1_PIN);
    servo2.attach(SERVO2_PIN);
    servo1.write(90);
    servo2.write(90);

    digitalWrite(LED1_VERDE_PIN, LOW);
    digitalWrite(LED1_VERMELHO_PIN, HIGH);
    digitalWrite(LED2_VERDE_PIN, LOW);
    digitalWrite(LED2_VERMELHO_PIN, HIGH);

    preferences.begin("biometria", false);
    Serial2.begin(57600, SERIAL_8N1, RXD2, TXD2);
    if (finger.verifyPassword())
    {
        Serial.println("Sensor biométrico OK.");
    }
    else
    {
        Serial.println("Sensor biométrico não encontrado.");
        lastStatusMessage = "ERRO: Sensor biométrico não encontrado!";
    }
}

void resetEnrollment()
{
    enrollmentState = IDLE;
    pending_id = 0;
    memset(&pending_user, 0, sizeof(UserData)); // Limpa a struct
}

void openDrawer(int drawerNumber, String userName)
{
    lastStatusMessage = "Gaveta " + String(drawerNumber) + " aberta por " + userName + ".";

    // --- Parâmetros do Alarme ---
    long total_open_time = 10000;  // Tempo total de 10 segundos
    int short_beep_duration = 100; // Duração de um "bip" (100ms)
    int pause_between_beeps = 100; // Pausa entre os dois bipes (100ms)
    int long_pause_duration = 1000;

    // --- Lógica para abrir a gaveta ---
    Servo *p_servo = (drawerNumber == 1) ? &servo1 : &servo2;
    int led_verde = (drawerNumber == 1) ? LED1_VERDE_PIN : LED2_VERDE_PIN;
    int led_vermelho = (drawerNumber == 1) ? LED1_VERMELHO_PIN : LED2_VERMELHO_PIN;

    digitalWrite(led_verde, HIGH);
    digitalWrite(led_vermelho, LOW);
    p_servo->write(0); // Abre a gaveta

    // --- Laço do Alarme (executa por 10 segundos) ---
    unsigned long startTime = millis();
    while (millis() - startTime < total_open_time)
    {

        digitalWrite(BUZZER_PIN, HIGH);
        delay(short_beep_duration);
        digitalWrite(BUZZER_PIN, LOW);
        delay(pause_between_beeps);
        digitalWrite(BUZZER_PIN, HIGH);
        delay(short_beep_duration);
        digitalWrite(BUZZER_PIN, LOW);

        delay(long_pause_duration);

        // Garante que o laço não execute um delay desnecessário no final
        if (millis() - startTime >= total_open_time)
        {
            break;
        }
    }

    // --- Lógica para fechar a gaveta ---
    p_servo->write(90);
    digitalWrite(led_verde, LOW);
    digitalWrite(led_vermelho, HIGH);
}

UserData loadUserData(int id)
{
    UserData user;
    char key[10];
    sprintf(key, "u%d", id);
    preferences.getBytes(key, &user, sizeof(UserData));
    return user;
}

void saveUserData(int id, UserData user)
{
    char key[10];
    sprintf(key, "u%d", id);
    preferences.putBytes(key, &user, sizeof(UserData));
}

int getFreeID()
{
    for (int i = 1; i <= 127; i++)
    {
        if (finger.loadModel(i) != FINGERPRINT_OK)
            return i;
    }
    return -1;
}

int getFingerprintIDez()
{
    uint8_t p = finger.getImage();
    if (p != FINGERPRINT_OK)
        return -1;
    p = finger.image2Tz();
    if (p != FINGERPRINT_OK)
        return -1;
    p = finger.fingerSearch();
    if (p != FINGERPRINT_OK)
        return -1;
    return finger.fingerID;
}