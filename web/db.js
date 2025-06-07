const mysql = require('mysql2');

const pool = mysql.createPool({
    host: 'localhost',     // Remova a porta daqui
    port: 3307,            // Adicione a porta como propriedade separada
    user: 'root',
    password: '',
    database: 'esp_biometria',
    waitForConnections: true,
    connectionLimit: 10,
    queueLimit: 0
});

module.exports = pool.promise();