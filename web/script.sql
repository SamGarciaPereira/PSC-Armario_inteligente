CREATE DATABASE IF NOT EXISTS esp_biometria;
USE esp_biometria;

    CREATE TABLE IF NOT EXISTS usuarios (
        id INT AUTO_INCREMENT PRIMARY KEY,
        nome VARCHAR(100) NOT NULL,
        matricula VARCHAR(20) NOT NULL UNIQUE,
        cpf VARCHAR(11) NOT NULL,
        data_cadastro TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    );  

    CREATE TABLE IF NOT EXISTS logs_biometria (
        id INT AUTO_INCREMENT PRIMARY KEY,
        matricula VARCHAR(20) NOT NULL,
        status VARCHAR(50) NOT NULL,
        origem VARCHAR(50) NOT NULL,
        data_hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (matricula) REFERENCES usuarios(matricula)
    );