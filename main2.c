#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include <ESP32Servo.h>

Preferences preferences;
Servo servo1;  // Servo para Gaveta 1
Servo servo2;  // Servo para Gaveta 2

struct UserData {
    char name[30];
    char cpf[12];
    char matricula[20];
    int drawer;  // Gaveta associada (1 ou 2)
};

#define RXD2 16
#define TXD2 17
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

// Protótipos de função
void initializeSensor();
void showMainMenu();
void handleMenu(char option);
void registerFingerprint();
void searchFingerprint();
void listDatabase();
void deleteFingerprint();
bool readSerialField(char* buffer, size_t maxLength);
void clearSerialBuffer();
void saveUserData(int id, UserData user);
UserData loadUserData(int id);
void showUserData(UserData user);
int getFreeID();
bool enrollFingerprint(int id);
int getFingerprintIDez();
int readNumber();
void openDrawer(int drawerNumber);
int selectDrawer();  // Nova função para seleção de gaveta

void setup() {
    Serial.begin(115200);
    Serial2.begin(57600, SERIAL_8N1, RXD2, TXD2);
    
    // Configuração dos servos
    servo1.attach(18); // Gaveta 1 no GPIO18
    servo2.attach(19); // Gaveta 2 no GPIO19
    servo1.write(0);   // Fecha gaveta 1
    servo2.write(0);   // Fecha gaveta 2
    
    preferences.begin("biometria", false);
    initializeSensor();
    showMainMenu();
}

void loop() {
    if (Serial.available()) {
        handleMenu(Serial.read());
    }
    delay(50);
}

void initializeSensor() {
    finger.begin(57600);
    
    if (finger.verifyPassword()) {
        Serial.println("\nSensor biometrico inicializado!");
        finger.getTemplateCount();
        Serial.print("Digitais cadastradas: ");
        Serial.println(finger.templateCount);
    } else {
        Serial.println("\nFalha na comunicacao com o sensor!");
        while(1);
    }
}

void showMainMenu() {
    Serial.println("\n\n=== MENU PRINCIPAL ===");
    Serial.println("1 - Cadastrar nova digital");
    Serial.println("2 - Buscar por digital");
    Serial.println("3 - Listar cadastros");
    Serial.println("4 - Remover cadastro");
    Serial.println("========================");
}

void handleMenu(char option) {
    switch(option) {
        case '1':
            registerFingerprint();
            break;
        case '2':
            searchFingerprint();
            break;
        case '3':
            listDatabase();
            break;
        case '4':
            deleteFingerprint();
            break;
        default:
            Serial.println("\nOpcao invalida!");
    }
    showMainMenu();
}

int selectDrawer() {
    Serial.println("\nSelecione a gaveta:");
    Serial.println("1 - Gaveta 1");
    Serial.println("2 - Gaveta 2");
    
    while(true) {
        if(Serial.available()) {
            char choice = Serial.read();
            if(choice == '1') {
                Serial.println("Gaveta 1 selecionada");
                return 1;
            } else if(choice == '2') {
                Serial.println("Gaveta 2 selecionada");
                return 2;
            } else {
                Serial.println("Opcao invalida! Digite 1 ou 2");
            }
        }
        delay(50);
    }
}

void registerFingerprint() {
    UserData user;
    memset(&user, 0, sizeof(user));
    
    Serial.println("\n=== NOVO CADASTRO ===");

    Serial.print("Nome: ");
    while(!readSerialField(user.name, sizeof(user.name)));
    
    Serial.print("CPF: ");
    while(!readSerialField(user.cpf, sizeof(user.cpf)));
    
    Serial.print("Matricula: ");
    while(!readSerialField(user.matricula, sizeof(user.matricula)));
    
    // Selecionar gaveta associada
    user.drawer = selectDrawer();

    int id = getFreeID();
    if(id == -1) {
        Serial.println("\nMemoria cheia!");
        return;
    }

    if(enrollFingerprint(id)) {
        saveUserData(id, user);
        Serial.println("\nCadastro realizado!");
        finger.getTemplateCount();
    } else {
        Serial.println("\nFalha no cadastro!");
    }
}

void searchFingerprint() {
    Serial.println("\nColoque o dedo no sensor...");
    int foundID = -1;
    unsigned long startTime = millis();
    
    while((millis() - startTime) < 30000) {
        foundID = getFingerprintIDez();
        if(foundID != -1) break;
        delay(100);
    }
    
    if(foundID != -1) {
        UserData user = loadUserData(foundID);
        showUserData(user);
        
        // Abrir gaveta associada ao usuário
        openDrawer(user.drawer);
    } else {
        Serial.println("\nDigital nao reconhecida!");
    }
}

void listDatabase() {
    finger.getTemplateCount();
    int count = finger.templateCount;
    
    if(count == 0) {
        Serial.println("\nNenhum cadastro encontrado!");
        return;
    }
    
    Serial.println("\n=== CADASTRADOS ===");
    for(int i = 1; i <= 127; i++) {
        if(finger.loadModel(i) == FINGERPRINT_OK) {
            UserData user = loadUserData(i);
            Serial.print("ID: ");
            Serial.print(i);
            Serial.print(" | Nome: ");
            Serial.print(user.name);
            Serial.print(" | CPF: ");
            Serial.print(user.cpf);
            Serial.print(" | Gaveta: ");
            Serial.println(user.drawer);
        }
    }
    Serial.println("===================");
}

void deleteFingerprint() {
    listDatabase();
    Serial.println("\nDigite o ID do cadastro a ser removido:");
    
    int id = readNumber();
    if(id < 1 || id > 127) {
        Serial.println("\nID invalido! (1-127)");
        return;
    }

    if(finger.loadModel(id) != FINGERPRINT_OK) {
        Serial.println("\nID nao encontrado!");
        return;
    }

    if(finger.deleteModel(id) == FINGERPRINT_OK) {
        char key[10];
        sprintf(key, "u%d", id);
        preferences.remove(key);
        
        Serial.print("\nCadastro ID ");
        Serial.print(id);
        Serial.println(" removido com sucesso!");
    } else {
        Serial.println("\nFalha ao remover digital!");
    }
}

bool readSerialField(char* buffer, size_t maxLength) {
    clearSerialBuffer();
    size_t index = 0;
    
    while(index < maxLength - 1) {
        if(Serial.available()) {
            char c = Serial.read();
            
            if(c == '\n' || c == '\r') {
                buffer[index] = '\0';
                Serial.println();
                return true;
            }
            
            if(isPrintable(c)) {
                buffer[index++] = c;
                Serial.write(c);
            }
        }
    }
    buffer[index] = '\0';
    return true;
}

void clearSerialBuffer() {
    while(Serial.available()) {
        Serial.read();
    }
}

void saveUserData(int id, UserData user) {
    char key[10];
    sprintf(key, "u%d", id);
    preferences.putBytes(key, &user, sizeof(UserData));
}

UserData loadUserData(int id) {
    UserData user;
    char key[10];
    sprintf(key, "u%d", id);
    preferences.getBytes(key, &user, sizeof(UserData));
    return user;
}

void showUserData(UserData user) {
    Serial.println("\n=== DADOS ===");
    Serial.print("Nome: "); Serial.println(user.name);
    Serial.print("CPF: "); Serial.println(user.cpf);
    Serial.print("Matricula: "); Serial.println(user.matricula);
    Serial.print("Gaveta: "); Serial.println(user.drawer);
    Serial.println("=============");
}

int getFreeID() {
    for(int i = 1; i <= 127; i++) {
        if(finger.loadModel(i) != FINGERPRINT_OK) {
            return i;
        }
    }
    return -1;
}

bool enrollFingerprint(int id) {
    int p = -1;
    
    Serial.println("Coloque o dedo no sensor...");
    while(p != FINGERPRINT_OK) {
        p = finger.getImage();
        if(p == FINGERPRINT_NOFINGER) continue;
        if(p != FINGERPRINT_OK) return false;
    }

    p = finger.image2Tz(1);
    if(p != FINGERPRINT_OK) return false;

    Serial.println("Retire o dedo");
    delay(2000);
    
    p = 0;
    while(p != FINGERPRINT_NOFINGER) {
        p = finger.getImage();
    }

    Serial.println("Coloque o mesmo dedo novamente");
    p = -1;
    while(p != FINGERPRINT_OK) {
        p = finger.getImage();
        if(p == FINGERPRINT_NOFINGER) continue;
        if(p != FINGERPRINT_OK) return false;
    }

    p = finger.image2Tz(2);
    if(p != FINGERPRINT_OK) return false;

    p = finger.createModel();
    if(p != FINGERPRINT_OK) return false;

    p = finger.storeModel(id);
    return (p == FINGERPRINT_OK);
}

int getFingerprintIDez() {
    int p = finger.getImage();
    if(p != FINGERPRINT_OK) return -1;

    p = finger.image2Tz();
    if(p != FINGERPRINT_OK) return -1;

    p = finger.fingerFastSearch();
    if(p != FINGERPRINT_OK) return -1;

    return finger.fingerID;
}

int readNumber() {
    char buffer[5];
    readSerialField(buffer, sizeof(buffer));
    return atoi(buffer);
}

void openDrawer(int drawerNumber) {
    if(drawerNumber != 1 && drawerNumber != 2) {
        Serial.println("Numero de gaveta invalido!");
        return;
    }
    
    Serial.print("\nAbrindo gaveta ");
    Serial.println(drawerNumber);
    
    if(drawerNumber == 1) {
        servo1.write(90);   // Abre gaveta 1 (90 graus)
        delay(5000);        // Mantém aberta por 5 segundos
        servo1.write(0);    // Fecha gaveta 1
    } else if(drawerNumber == 2) {
        servo2.write(90);   // Abre gaveta 2 (90 graus)
        delay(5000);        // Mantém aberta por 5 segundos
        servo2.write(0);    // Fecha gaveta 2
    }
    
    Serial.print("Gaveta ");
    Serial.print(drawerNumber);
    Serial.println(" fechada!");
}