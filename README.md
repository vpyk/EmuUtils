# Emu80 utilities

## bin2tape

### Назначение
Утилита командной строки bin2tape служит для формирования файлов образов лент (и не только) компьютеров, поддерживаемых эмулятором Emu80. Позволяет из двоичных файлов формировать rk (rkr/rkp/kra/rk8/rku/rke/rkl), rks, rko, bru/ord, cas, lvt. В качестве параметров принимает имя исходного двоичного файла, начальный адрес, для некоторых форматов также адрес запуска и внутреннее имя файла. Будет полезна для разработчиков, пишущих под поддерживаемые компьютеры, для автоматизации формирования образа ленты после компиляции.

### Бинарные сборки
* Сборка под Windows: [https://emu80.org/files/?id=78](https://emu80.org/files/?id=78)

### Компиляция под linux и т. п.
    g++ bin2tape.cpp --std=c++11 -o bin2tape
(зависимости отсутствуют)

## rkdisk

### Назначение
Утилита командной строки для работы с образами РК ДОС. Позволяет создавать и форматировать образы дисков, просматривать содержимое образов,  добавлять, извелкать и удалять файлы, устанавливать атрибуты.

### Бинарные сборки
* Сборка под Windows: [https://emu80.org/files/?id=81](https://emu80.org/files/?id=81)

### Компиляция под linux и т. п.
    g++ rkdisk.cpp rkimage/*.cpp --std=c++14 -o rkdisk
(зависимости отсутствуют)

## rdihfetools

### Назначение
Rdi HFE tools. Набор из двух утилит *hfe2rdi* и *rdi2hfe* на Python. Служат для преобразования образов дисков РК ДОС (rdi, rkdisk), используемых в эмуляторах Emu80 и "Ретро КР580", в формат или из формата HFE образа эмулятора дисковода Gotek и его аналогов.

### Скачать архив с последней версией
* Архив: [https://emu80.org/files/?id=82](https://emu80.org/files/?id=82)

### Зависимости
python >= 3.6

## bsm2txt

### Назначение
Утилита служит для преобразования файлов Basic Micron в текстовые файлы. Результирующий файл имеет кодировку DOS (cp866). 

### Скачать архив с последней версией
* Сборка под Windows: [https://emu80.org/files/?id=83](https://emu80.org/files/?id=83)

### Компиляция под linux etc. 
    fpc -Sd bsm2txt.pas
