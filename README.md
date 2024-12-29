# modbus
Совместная работа с https://github.com/Revenan7

## Ruuun
1) откройте CMD и скопируйте проект
```
git clone https://github.com/kreipikc/modbus-request-response.git
```
2) перейдите в директорию проекта:
```
cd modbus
```
3) выполните:
```
docker-compose up --build -d
```
у вас должен быть запущен докер*

## Использование
1) зайдите в CMD и зайдите в консоль контейнера (убедитесь что все контейнеры запущены):
```
docker exec -it modbus-modbus-client-1 /bin/bash
```
2) запустите файл
```
./start
```


## про команды
![image](https://github.com/user-attachments/assets/f4e90ced-3507-43fe-be63-04ac29320c6c)
### 1) создание новой команды
   
![image](https://github.com/user-attachments/assets/4c27ed6c-36de-4183-91be-bb02f6a01add)

первая цифра - код фунцкии
```
1 - Read Coil Status

2 - Read Input Status

3 - Read Holding Registers

4 - Read Input Registers
```
вторая цифра - с какого регистра считывать

третья цифра - сколько регистров считывать

пример:

`4 9 1`

программа создаст функцию, которая считывает Input Register по адресу 9

### 2) запуск команды по ID

  а) введите IP адрес устройства, доступный в локальной сети
  
  б) введите порт этого устройства
  
  в) введите номер команды (доступные команды, которые вы создали можно посмотреть)
  
  если всё прошло успешно:
  
  ![image](https://github.com/user-attachments/assets/a64af077-3360-4a51-a036-0eabc1756304)
  
  (данная функция, прочла Input Register по адрессу 9, в устройстве доступный по IP 172.18.0.3 и порту 5020)

### 3) вывод всех доступных команд (которые вы создали ранее)
пример:

![image](https://github.com/user-attachments/assets/f2847aef-06d4-4c9c-b97e-3291c66d15ef)


### 4) вывод логов, всех отрпавленых пакетов и принятых соответсвенно
пример:

![image](https://github.com/user-attachments/assets/d3923b77-345c-4fa7-921a-04ee5b65b1ca)


## Выгрузка данных
1) откройте консоль Git Bash и зайдите в директорию проекта
```
cd modbus
```
2) зайдите в терминал запущенного контейнера modbus-dbpostgres-1
```
docker exec -it modbus-dbpostgres-1 bash
```
3) выполните:
```
psql -U postgres -d postgres -c "Copy (Select * From responses) To STDOUT With CSV HEADER DELIMITER ',';" > db.csv
```
4) нажмите Ctrl + D или впишите exit, чтобы выйти из консоли контейнера

5) впишите
```
docker cp modbus-dbpostgres-1:/db.csv ./db.csv
```

в папке проекта создастся файл

![image](https://github.com/user-attachments/assets/28b18f63-c835-43fc-a007-8df2ffd11289)

