#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include <ESP32Servo.h>

// --- CONFIGURAÇÕES DE REDE ---
const char *ssid = "Rede Oculta";  // <-- COLOQUE O NOME DA SUA REDE AQUI
const char *password = "20145600"; // <-- COLOQUE A SENHA DA SUA REDE AQUI

// --- DEFINIÇÃO DOS PINOS ---
#define LED1_VERDE_PIN 23
#define LED1_VERMELHO_PIN 4
#define LED2_VERDE_PIN 21
#define LED2_VERMELHO_PIN 5
#define BUZZER_PIN 22

// --- OBJETOS E VARIÁVEIS GLOBAIS ---
WebServer server(80);
Preferences preferences;
Servo servo1;
Servo servo2;

struct UserData
{
    char name[50];      
    char cpf[20];       
    char matricula[20]; 
    int drawer;
};

//Variáveis para controlar o estado do cadastro
bool registration_mode = false;
int registration_id = 0;
UserData pending_user;

#define RXD2 16
#define TXD2 17
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

// --- PROTÓTIPOS DE FUNÇÕES ---
void initializeSensor();
void openDrawer(int drawerNumber);
UserData loadUserData(int id);
void saveUserData(int id, UserData user);
int getFreeID();
bool enrollFingerprint(int id);
int getFingerprintIDez();
void showUserData(UserData user);
bool isDataDuplicate(const char *cpf, const char *matricula); // NOVO: Protótipo da função de verificação

void handleRoot();
void handleList();
void handleRegisterForm();
void handleDoRegister();
void handleDelete();
String getHtmlHeader(String title);

// --- SETUP ---
void setup()
{
    Serial.begin(115200);
    Serial2.begin(57600, SERIAL_8N1, RXD2, TXD2);

    // Configuração dos pinos
    pinMode(LED1_VERDE_PIN, OUTPUT);
    pinMode(LED1_VERMELHO_PIN, OUTPUT);
    pinMode(LED2_VERDE_PIN, OUTPUT);
    pinMode(LED2_VERMELHO_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // Configuração dos servos
    servo1.attach(18);
    servo2.attach(19);
    servo1.write(90);
    servo2.write(90);

    // Define o estado inicial dos LEDs (lógica invertida para Gaveta 1)
    digitalWrite(LED1_VERDE_PIN, LOW);
    digitalWrite(LED1_VERMELHO_PIN, HIGH);
    digitalWrite(LED2_VERDE_PIN, LOW);
    digitalWrite(LED2_VERMELHO_PIN, HIGH);

    preferences.begin("biometria", false);
    initializeSensor();

    // Conectar ao WiFi
    Serial.print("\nConectando ao WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado!");
    Serial.print("IP do servidor: http://");
    Serial.println(WiFi.localIP());

    // Rotas do servidor web
    server.on("/", HTTP_GET, handleRoot);
    server.on("/list", HTTP_GET, handleList);
    server.on("/register", HTTP_GET, handleRegisterForm);
    server.on("/doregister", HTTP_POST, handleDoRegister);
    server.on("/delete", HTTP_GET, handleDelete);

    server.begin();
    Serial.println("Servidor web iniciado.");
}

// --- LOOP ---
// ALTERADO: O loop agora gerencia o modo de cadastro
void loop()
{
    server.handleClient();

    // Se o modo de cadastro foi ativado pela página web...
    if (registration_mode)
    {
        Serial.printf("\n\n=== INICIANDO CADASTRO PARA ID %d ===\n", registration_id);
        Serial.println("Siga as instruções abaixo...");

        if (enrollFingerprint(registration_id))
        {
            saveUserData(registration_id, pending_user);
            Serial.println("\n>>> Cadastro realizado com sucesso! <<<");
        }
        else
        {
            Serial.println("\n>>> Falha no cadastro da digital! Tente novamente a partir da página web. <<<");
        }
        // Reseta o modo de cadastro para voltar à operação normal
        registration_mode = false;
    }
    else
    {
        // Operação normal: verifica se uma digital foi colocada para abrir a gaveta
        int foundID = getFingerprintIDez();
        if (foundID != -1)
        {
            Serial.print("Digital encontrada: ID ");
            Serial.println(foundID);
            UserData user = loadUserData(foundID);
            showUserData(user);
            openDrawer(user.drawer);
        }
    }
    delay(50);
}

// =======================================================
//  FUNÇÕES DE INTERFACE WEB (HANDLERS)
// =======================================================

String getHtmlHeader(String title)
{
    String header = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>" + title + "</title>";
    header += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    header += "<style>body{font-family: Arial, sans-serif; margin: 20px;} h1{color: #333;} a{text-decoration: none;}";
    header += "button{background-color: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-bottom: 10px;}";
    header += "button:hover{background-color: #0056b3;} table{width: 100%; border-collapse: collapse;} th, td{padding: 8px; text-align: left; border-bottom: 1px solid #ddd;}";
    header += "form label{display: block; margin-top: 10px;} input, select{width: 90%; padding: 8px; margin-top: 5px;}";
    header += ".container{max-width: 600px; margin: auto;} .back-button{background-color: #6c757d;} .back-button:hover{background-color: #5a6268;}</style>";
    header += "</head><body><div class='container'><h1>" + title + "</h1>";
    return header;
}

String getHtmlFooter()
{
    return "</div></body></html>";
}

void handleRoot()
{
    String html = getHtmlHeader("Menu Principal - Controle Biométrico");
    html += "<a href='/register'><button>Cadastrar Nova Digital</button></a><br>";
    html += "<a href='/list'><button>Listar Cadastros</button></a><br>";
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleList()
{
    String html = getHtmlHeader("Usuários Cadastrados");
    html += "<table><tr><th>ID</th><th>Nome</th><th>CPF</th><th>Matrícula</th><th>Gaveta</th><th>Ação</th></tr>";

    finger.getTemplateCount();
    if (finger.templateCount == 0)
    {
        html += "<tr><td colspan='6'>Nenhum cadastro encontrado.</td></tr>";
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
                html += "<td><a href='/delete?id=" + String(i) + "' onclick='return confirm(\"Tem certeza?\");'>Remover</a></td>";
                html += "</tr>";
            }
        }
    }
    html += "</table><br>";
    html += "<a href='/'><button class='back-button'>Voltar ao Menu</button></a>";
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleRegisterForm()
{
    String html = getHtmlHeader("Novo Cadastro");
    html += "<form action='/doregister' method='POST'>";
    html += "<label for='name'>Nome:</label><input type='text' id='name' name='name' required>";
    html += "<label for='cpf'>CPF:</label><input type='text' id='cpf' name='cpf' required>";
    html += "<label for='matricula'>Matrícula:</label><input type='text' id='matricula' name='matricula' required>";
    html += "<label for='drawer'>Gaveta:</label><select id='drawer' name='drawer'><option value='1'>Gaveta 1</option><option value='2'>Gaveta 2</option></select>";
    html += "<br><br><button type='submit'>Iniciar Cadastro</button>";
    html += "</form>";
    html += "<a href='/'><button class='back-button'>Cancelar</button></a>";
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

// ALTERADO: Função completamente reestruturada
void handleDoRegister()
{
    if (registration_mode)
    {
        server.send(503, "text/html", "<h1>Sistema ocupado com outro cadastro. Tente novamente em alguns instantes.</h1><a href='/'>Voltar</a>");
        return;
    }

    String newCpf = server.arg("cpf");
    String newMatricula = server.arg("matricula");

    // NOVO: Verificação de dados duplicados
    if (isDataDuplicate(newCpf.c_str(), newMatricula.c_str()))
    {
        String html = getHtmlHeader("Erro no Cadastro");
        html += "<p>Erro: CPF ou Matrícula já cadastrado no sistema.</p>";
        html += "<a href='/register'><button class='back-button'>Tentar Novamente</button></a>";
        html += getHtmlFooter();
        server.send(409, "text/html", html); // 409 Conflict
        return;
    }

    int id = getFreeID();
    if (id == -1)
    {
        server.send(500, "text/html", "<h1>Erro: Memória cheia!</h1><a href='/'>Voltar</a>");
        return;
    }

    // Armazena os dados do usuário e o ID em variáveis globais
    strncpy(pending_user.name, server.arg("name").c_str(), sizeof(pending_user.name) - 1);
    strncpy(pending_user.cpf, newCpf.c_str(), sizeof(pending_user.cpf) - 1);
    strncpy(pending_user.matricula, newMatricula.c_str(), sizeof(pending_user.matricula) - 1);
    pending_user.drawer = server.arg("drawer").toInt();
    registration_id = id;

    // Ativa o modo de registro
    registration_mode = true;

    // Envia uma resposta para o navegador instruindo o usuário
    String html = getHtmlHeader("Processando Cadastro...");
    html += "<p><b>Cadastro iniciado!</b></p>";
    html += "<p>Por favor, olhe para o Monitor Serial da sua IDE Arduino e siga as instruções para cadastrar a digital.</p>";
    html += "<p>O sistema voltará ao normal após o fim do processo.</p>";
    html += "<a href='/'><button class='back-button'>Voltar ao Menu Principal</button></a>";
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleDelete()
{
    String id_str = server.arg("id");
    if (id_str == "")
    {
        server.send(400, "text/html", "<h1>Erro: ID não fornecido.</h1><a href='/list'>Voltar</a>");
        return;
    }

    int id = id_str.toInt();
    if (finger.deleteModel(id) == FINGERPRINT_OK)
    {
        char key[10];
        sprintf(key, "u%d", id);
        preferences.remove(key);

        String html = getHtmlHeader("Sucesso");
        html += "<p>Cadastro com ID " + String(id) + " foi removido.</p>";
        html += "<a href='/list'><button class='back-button'>Voltar para Lista</button></a>";
        server.send(200, "text/html", html);
    }
    else
    {
        String html = getHtmlHeader("Erro");
        html += "<p>Falha ao remover o cadastro com ID " + String(id) + ".</p>";
        html += "<a href='/list'><button class='back-button'>Voltar para Lista</button></a>";
        server.send(200, "text/html", html);
    }
}

// =======================================================
//  FUNÇÕES DO SISTEMA BIOMÉTRICO
// =======================================================

void initializeSensor()
{
    finger.begin(57600);
    if (finger.verifyPassword())
    {
        Serial.println("\nSensor biometrico inicializado!");
        finger.getTemplateCount();
        Serial.print("Digitais cadastradas: ");
        Serial.println(finger.templateCount);
    }
    else
    {
        Serial.println("\nFalha na comunicacao com o sensor!");
        while (1)
            delay(1);
    }
}

// NOVO: Função para verificar se CPF ou Matrícula já existem
bool isDataDuplicate(const char *cpf, const char *matricula)
{
    for (int i = 1; i <= 127; i++)
    {
        if (finger.loadModel(i) == FINGERPRINT_OK)
        {
            UserData existingUser = loadUserData(i);
            if (strcmp(existingUser.cpf, cpf) == 0 || strcmp(existingUser.matricula, matricula) == 0)
            {
                return true; // Encontrou um duplicado
            }
        }
    }
    return false; // Nenhum duplicado encontrado
}

void openDrawer(int drawerNumber)
{
    if (drawerNumber != 1 && drawerNumber != 2)
    {
        Serial.println("Numero de gaveta invalido!");
        return;
    }

    Serial.print("\nAbrindo gaveta ");
    Serial.println(drawerNumber);

    if (drawerNumber == 1)
    {
        // Lógica invertida para a Gaveta 1
        digitalWrite(LED1_VERDE_PIN, HIGH);
        digitalWrite(LED1_VERMELHO_PIN, LOW);
        digitalWrite(BUZZER_PIN, HIGH);

        servo1.write(0);
        delay(15000);
        servo1.write(90);

        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED1_VERDE_PIN, LOW);
        digitalWrite(LED1_VERMELHO_PIN, HIGH);
    }
    else if (drawerNumber == 2)
    {
        // Lógica original para a Gaveta 2
        digitalWrite(LED2_VERMELHO_PIN, LOW);
        digitalWrite(LED2_VERDE_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);

        servo2.write(0);
        delay(15000);
        servo2.write(90);

        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED2_VERDE_PIN, LOW);
        digitalWrite(LED2_VERMELHO_PIN, HIGH);
    }

    Serial.print("Gaveta ");
    Serial.print(drawerNumber);
    Serial.println(" fechada!");
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
        {
            return i;
        }
    }
    return -1;
}

bool enrollFingerprint(int id)
{
    int p = -1;
    Serial.println("Coloque o dedo no sensor...");
    while (p != FINGERPRINT_OK)
    {
        p = finger.getImage();
        if (p == FINGERPRINT_NOFINGER)
        {
            server.handleClient(); // Permite que o servidor web continue respondendo
            delay(50);
            continue;
        }
        if (p != FINGERPRINT_OK)
            return false;
    }
    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK)
        return false;

    Serial.println("Retire o dedo");
    delay(2000);
    p = 0;
    while (p != FINGERPRINT_NOFINGER)
    {
        p = finger.getImage();
        server.handleClient();
        delay(50);
    }

    Serial.println("Coloque o mesmo dedo novamente");
    p = -1;
    while (p != FINGERPRINT_OK)
    {
        p = finger.getImage();
        if (p == FINGERPRINT_NOFINGER)
        {
            server.handleClient();
            delay(50);
            continue;
        }
        if (p != FINGERPRINT_OK)
            return false;
    }
    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK)
        return false;

    p = finger.createModel();
    if (p != FINGERPRINT_OK)
        return false;

    p = finger.storeModel(id);
    return (p == FINGERPRINT_OK);
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

void showUserData(UserData user)
{
    Serial.println("\n=== DADOS DO USUARIO ===");
    Serial.print("Nome: ");
    Serial.println(user.name);
    Serial.print("CPF: ");
    Serial.println(user.cpf);
    Serial.print("Matricula: ");
    Serial.println(user.matricula);
    Serial.print("Gaveta: ");
    Serial.println(user.drawer);
    Serial.println("========================");
}
