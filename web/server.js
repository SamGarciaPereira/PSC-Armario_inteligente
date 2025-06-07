const express = require('express');
const bodyParser = require('body-parser');
const cors = require('cors');
const pool = require('./db');
const path = require('path');

const app = express();
const PORT = 3000;

// Configuração simplificada do CORS para desenvolvimento
app.use(cors({
    origin: true,
    credentials: true
}));

// Middlewares
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

// Conexão com o banco de dados
pool.getConnection()
    .then(conn => {
        console.log('Conexão com o banco estabelecida!');
        conn.release();
    })
    .catch(err => {
        console.error('Erro ao conectar ao banco:', err);
        process.exit(1);
    });

// Rotas corrigidas (atenção aos parâmetros)
app.post('/api/biometria', async (req, res) => {
    try {
        const { matricula, status } = req.body;
        if (!matricula || !status) {
        return res.status(400).json({ error: 'Dados inválidos' });
        }

        await pool.execute(
        'INSERT INTO logs_biometria (matricula, status, origem) VALUES (?, ?, ?)',
        [matricula, status, 'ESP32']
        );
        res.json({ success: true, message: 'Biometria registrada' });
    } catch (err) {
        console.error('Erro:', err);
        res.status(500).json({ error: 'Erro interno' });
    }
});

app.post('/api/usuarios', async (req, res) => {
    try {
        const { nome, matricula, cpf } = req.body;
        
        if (!nome || !matricula || !cpf) {
        return res.status(400).json({ error: 'Preencha todos os campos' });
        }

        const [result] = await pool.execute(
        'INSERT INTO usuarios (nome, matricula, cpf) VALUES (?, ?, ?)',
        [nome, matricula, cpf]
        );
        res.json({ success: true, message: 'Usuário cadastrado' });
    } catch (err) {
        console.error('Erro:', err);
        res.status(500).json({ error: err.code === 'ER_DUP_ENTRY' ? 'Matrícula já existe' : 'Erro interno' });
    }
});

// Rota de teste corrigida
app.get('/test', (req, res) => {
    res.send('Servidor operacional');
});

// Rota raiz
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, () => {
    console.log(`Servidor rodando na porta ${PORT}`);
});