#include <iostream>
#include <pqxx/pqxx>
#include <modbus/modbus.h>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <stdexcept>


void createTablesIfNotExist(pqxx::connection& conn) 
{
    pqxx::work txn(conn);

    // Проверка наличия таблицы commands
    pqxx::result r = txn.exec("SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name='commands')");
    bool commandsTableExists = r[0][0].as<bool>();

    if (!commandsTableExists) 
    {
        std::cout << "Creating table 'commands'..." << std::endl;
        txn.exec("CREATE TABLE commands (id SERIAL PRIMARY KEY, functionCode INT NOT NULL, startingAddress INT NOT NULL, quantityOfRegisters INT NOT NULL)");
    }

    // Проверка наличия таблицы responses
    r = txn.exec("SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name='responses')");
    bool responsesTableExists = r[0][0].as<bool>();

    if (!responsesTableExists) 
    {
        std::cout << "Creating table 'responses'..." << std::endl;
        txn.exec("CREATE TABLE responses (id SERIAL PRIMARY KEY, command BYTEA NOT NULL, response BYTEA NOT NULL)");
    }

    txn.commit();
}


void addCommandToDB(pqxx::connection& conn, int functionCode, int startingAddress, int quantityOfRegisters) 
{
    pqxx::work txn(conn);
    txn.exec_params("INSERT INTO commands (functionCode, startingAddress, quantityOfRegisters) VALUES ($1, $2, $3)", functionCode, startingAddress, quantityOfRegisters);
    txn.commit();
}


std::vector<uint8_t> createModbusPacket(uint8_t functionCode, uint16_t startingAddress, uint16_t quantityOfRegisters)
{
    std::vector<uint8_t> packet = {
        0x01,
        functionCode,
        (uint8_t)((startingAddress >> 8) & 0xFF),
        (uint8_t)(startingAddress & 0xFF),
        (uint8_t)((quantityOfRegisters >> 8) & 0xFF),
        (uint8_t)(quantityOfRegisters & 0xFF)
    };
    return packet;
}


std::vector<uint8_t> getCommandFromDB(pqxx::connection& conn, int commandId) 
{
    pqxx::work txn(conn);
    pqxx::row res = txn.exec_params1("SELECT functionCode, startingAddress, quantityOfRegisters FROM commands WHERE id = $1", commandId);

    int tempFunc = res[0].as<int>();
    uint8_t functionCode = static_cast<uint8_t>(tempFunc);

    int tempAddr = res[1].as<int>();
    uint16_t startingAddress = static_cast<uint16_t>(tempAddr);

    int tempQty = res[2].as<int>();
    uint16_t quantityOfRegisters = static_cast<uint16_t>(tempQty);

    return createModbusPacket(functionCode, startingAddress, quantityOfRegisters);
}


std::vector<uint8_t> RequestAndResponseModbus(const std::string ip, int port, const std::vector<uint8_t>& packet) 
{
    modbus_t* ctx = modbus_new_tcp(ip.c_str(), port);
    if (modbus_connect(ctx) == -1)
    {
        std::cerr << "Unable to connect to Modbus server" << std::endl;
        modbus_free(ctx);
        return {};
    }

    int request_length = modbus_send_raw_request(ctx, packet.data(), packet.size());
    if (request_length == -1) 
    {
        std::cerr << "Failed to send raw request" << std::endl;
        modbus_free(ctx);
        return {};
    }

    uint8_t raw_resp[MODBUS_TCP_MAX_ADU_LENGTH];
    int response_length = modbus_receive_confirmation(ctx, raw_resp);
    if (response_length == -1) 
    {
        std::cerr << "Failed to receive raw response" << std::endl;
        modbus_free(ctx);
        return {};
    }
    std::cout << "Команда полностью прошла успешно." << std::endl;

    for (int i = 0; i < response_length; i++) 
    {
        printf("%02X ", raw_resp[i]);
    }

    std::cout << std::endl;

    modbus_free(ctx);
    return std::vector<uint8_t>(raw_resp, raw_resp + response_length);
}


void saveModbusToDB(pqxx::connection& conn, const std::vector<uint8_t>& command, const std::vector<uint8_t>& response) 
{
    pqxx::work txn(conn);

    // std::vector<uint8_t> -> std::string
    auto bytesToHexString = [](const std::vector<uint8_t>& bytes) {
        std::stringstream hex_string;
        for (uint8_t byte : bytes) {
            hex_string << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(byte);
        }
        return hex_string.str();
    };

    std::string commandHex = bytesToHexString(command);
    std::string responseHex = bytesToHexString(response);

    // Вставка данных в таблицу
    txn.exec_params("INSERT INTO responses (command, response) VALUES (decode($1, 'hex'), decode($2, 'hex'))", commandHex, responseHex);
    txn.commit();
}


void processCommand(pqxx::connection& conn, int commandId, const std::string ip, int port) 
{
    std::vector<uint8_t> packet = getCommandFromDB(conn, commandId);                // Создаем пакет с данными из БД по commandId
    std::vector<uint8_t> response = RequestAndResponseModbus(ip, port, packet);     // Отправляем пакет и получаем ответ по Modbus
    saveModbusToDB(conn, packet, response);                                         // Сохраняем пакет (комманда) и ответ в БД
}


void PrintAll(pqxx::connection& conn) 
{
    pqxx::work txn(conn);
    
    // Запрос на получение всех команд из таблицы commands
    pqxx::result r = txn.exec("SELECT id, functionCode, startingAddress, quantityOfRegisters FROM commands");
    
    // Проверка на пустоту результата
    if (r.empty()) 
    {
        std::cout << "Нет доступных команд в базе данных." << std::endl;
        return;
    }

    // Вывод всех команд
    std::cout << "Доступные команды:\n";
    for (const auto& row : r) {
        int id = row[0].as<int>();
        int functionCode = row[1].as<int>();
        int startingAddress = row[2].as<int>();
        int quantityOfRegisters = row[3].as<int>();
        
        std::cout << "ID: " << id << ", Функция: " << functionCode << ", Начальный регистр: " << startingAddress << ", Количество регистров: " << quantityOfRegisters << std::endl;
    }
}

void debug(pqxx::connection& conn) 
{
    pqxx::work txn(conn);
    
    // Запрос для получения всех записей из таблицы responses
    pqxx::result r = txn.exec("SELECT id, command, response FROM responses");

    // Проверка на пустоту результата
    if (r.empty()) 
    {
        std::cout << "Нет данных в таблице 'responses'." << std::endl;
        return;
    }

    // Вывод всех записей из таблицы
    std::cout << "Данные из таблицы 'responses':\n";
    for (const auto& row : r) 
    {
        int id = row[0].as<int>();
        std::string commandHex = row[1].as<std::string>();
        std::string responseHex = row[2].as<std::string>();

        std::cout << "ID: " << id << ", Команда (в hex): " << commandHex << ", Ответ (в hex): " << responseHex << std::endl;
    }
}


int main() 
{
    try 
    { 
        pqxx::connection conn("dbname=postgres user=postgres password=postgres host=dbpostgres port=5432");
        createTablesIfNotExist(conn);

        while (true) {
            std::cout << std::endl << "Для выбора команды напишите её номер:" << std::endl;
            std::cout << "1. Создание новой команды" << std::endl;
            std::cout << "2. Запуск команды по ID" << std::endl;
            std::cout << "3. Вывод доступных команд" << std::endl;
            std::cout << "4. Вывод логов" << std::endl;


            std::string Command;
            std::cin >> Command;


            if (Command == "1") 
            {
                int functionCode, startingAddress, quantityOfRegisters;
                std::cout << "Введите данные для команды, а именно:\nКод функции, стартовый адрес регистра, количество регистров для считывания.\n\nИменно в таком порядке!" << std::endl;
                std::cin >> functionCode >> startingAddress >> quantityOfRegisters;
                addCommandToDB(conn, functionCode, startingAddress, quantityOfRegisters);
                 std::cout << "Команда успешно добавленна." << std::endl;
            }
            else if (Command == "2") 
            {
                int idCommand{}, port{};
                std::string ip;

                std::cout << "Введите IP устройства: " << std::endl;
                std::cin >> ip;
                std::cout << "Введите PORT устройства: " << std::endl;
                std::cin >> port;

                std::cout << "Введите номер (ID) команды: ";
                std::cin >> idCommand;
                //processCommand(conn, 1, ip, port);
                processCommand(conn, idCommand, ip, port);
            }
            else if (Command == "3") 
            {
                PrintAll(conn); 
            }
            else if (Command == "4") 
            {
                debug(conn);
            }
            else 
            {
                std::cout << "Завершение программы." << std::endl;
                break;
            }
        }
    } 
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}