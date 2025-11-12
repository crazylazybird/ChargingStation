#include "main.h"


const unsigned long PAYMENT_TIMEOUT = 800000;  // Таймаут в миллисекундах


extern volatile int operationNumber;
extern uint8_t receiveBuffer[BUFFER_SIZE];
extern int bufferIndex;

//для charging_management
long refundAmount;
long amountFIN;
int opNumberPrev;


const byte START_BYTE = 0x1F;                       // Стартовый байт
const byte PROTOCOL_DISCRIMINATOR_HIGH = 0x96;      // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;  // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;       // Дискриминатор протокола (младшие биты)
const byte MESSAGE_ID_IDL = 0x01;                   // ID сообщения IDL

bool FundingFLAG = false;

tlv receivedTLV {receivedTLV.mesName = "", receivedTLV.opNumber = 0, receivedTLV.amount = 0, receivedTLV.lastTime = millis(), receivedTLV.isMesProcessed = true};
tlv sentTLV{"", 0, 0, millis(), true};

transactions payment {payment.kWattPerHourAvailable = 0, payment.paidMinor = 0, payment.paymentStatus = WAITING_PAYMENT, payment.paymentStatusPrev = WAITING_PAYMENT, payment.lastTime = millis(), payment.chargingStatus = WAITING_TO_CHARGE, payment.chargingStatusPrev = WAITING_TO_CHARGE};


void start_payment(long amount) {
   
    stayIDLE = false;
    UART0_DEBUG_PORT.print("Начата оплата на сумму: ");
    UART0_DEBUG_PORT.print(long(amount) / 100.0);
    UART0_DEBUG_PORT.println(" руб.");
    sentTLV.amount = amount;
    // Отправляем терминал в режим приема оплаты

    send_VRP(amount);
}

/*В этой функции мы обрабатываем различные сообщения от терминала по сценариям
Когда пользователь выбирает на экране сумму к оплате и хочет зарядить электромобиль, то терминал посылает сообщение STA в ESP32 о том что есть желание у пользователя оплатить.
Чтобы терминал на своем экране предложил именно оплатить картой, т.е начать операцию перевода денег на банковский счет, то нужно отправить терминалу сообщение VRP с суммой.
Если пользователь оплатил, то терминал отправляет сообщение VRP об успешной оплате с той же суммой и тогда можно уже начать зарядку и установить переменную payment.paymentStatus в статус PAID
и затем производить обработку различных сценариев при зарядке в функции charging_managment()
*/
void processing_received_POS_message() {
      int lastOperationNumber = get_current_operation_number();
      long sentAmount = 0;

  if (receivedTLV.isMesProcessed) return;      // одно сообщение — одно действие

  const unsigned long now = millis();

/*
Получено сообщение о том что пользователь желает внести оплату, но это только начало
В теле условия мы должны отправить сообщение VRP с суммой 
*/
  if (receivedTLV.mesName == "STA") {
    receivedTLV.isMesProcessed = true;         // пометить до сайд-эффектов
    if (receivedTLV.amount > 0) {
      // фиксируем "контекст отправки" (для сравнения при ответе)
      //DEBUG - добавлена одна следующая строчка
      receivedTLV.amount   = receivedTLV.amount/100.0;
      sentTLV.amount   = receivedTLV.amount;
      sentTLV.mesName  = "VRP";
      sentTLV.lastTime = now;

      start_payment(sentTLV.amount);                   // внутри сформирует и отправит VRP
      
      int lastOperationNumber = get_current_operation_number();
    } else {
      UART0_DEBUG_PORT.println("STA без суммы — VRP не отправляем");
    }
    return;
  }

  // 2) Ответ по оплате: имя может быть VRP/RES/VRA (в зависимости от прошивки)
  if (receivedTLV.mesName == "VRP") {
    
    if (receivedTLV.amount == sentTLV.amount){
      UART0_DEBUG_PORT.print("Оплата подтверждена суммой: ");
      UART0_DEBUG_PORT.println(receivedTLV.amount);
      if (lastOperationNumber == get_current_operation_number()) {
        UART0_DEBUG_PORT.print("Номер операции совпадает: "); UART0_DEBUG_PORT.println(get_current_operation_number());
        handle_successful_payment();                    // пополняем счет и вычисляем оплаченный объем энергии
      }
      else {
        UART0_DEBUG_PORT.print("Номер операции запроса оплаты: "); 
        UART0_DEBUG_PORT.print(lastOperationNumber);
        UART0_DEBUG_PORT.print(" не совпадает с номером операции: "); 
        UART0_DEBUG_PORT.println(get_current_operation_number());
        handle_failed_payment();
        stayIDLE = true;                                     // при необходимости
      }
    } else {
      UART0_DEBUG_PORT.print("Несовпадение суммы (ожидали ");
      UART0_DEBUG_PORT.print(sentTLV.amount);
      UART0_DEBUG_PORT.print(", получили ");
      UART0_DEBUG_PORT.print(receivedTLV.amount);
      UART0_DEBUG_PORT.println(") — отклоняем");
      handle_failed_payment();
      stayIDLE = true;                                     // при необходимости
    }
    receivedTLV.isMesProcessed = true;                 // пометить до сайд-эффектов
    // очистка контекста отправленного VRP
    sentTLV.amount   = 0;
    sentTLV.mesName  = "";
    sentTLV.lastTime = 0;
    sentTLV.opNumber = 0;
    return;
  }

  // 3) Прочие сообщения — пометить обработанными
  receivedTLV.isMesProcessed = true;

  // 4) Таймаут ожидания ответа на наш VRP
  if (sentTLV.amount > 0 && (now - sentTLV.lastTime) > PAYMENT_TIMEOUT) {
    UART0_DEBUG_PORT.println("Payment timeout");
    handle_payment_timeout();
    sentTLV.amount   = 0;
    sentTLV.mesName  = "";
    sentTLV.lastTime = 0;
    sentTLV.opNumber = 0;
  }
}




void process_POS_received_data() {
  if (bufferIndex == 0) return;


  if (bufferIndex < 7) { clear_buffer(); return; }


  if (receiveBuffer[0] != START_BYTE) { clear_buffer(); return; }


  uint16_t msgLen = (uint16_t(receiveBuffer[1]) << 8) | receiveBuffer[2];
  uint16_t proto  = (uint16_t(receiveBuffer[3]) << 8) | receiveBuffer[4];


  if (1 + 2 + msgLen + 2 != bufferIndex) {
    UART0_DEBUG_PORT.println("Ошибка: длина кадра не совпала");
    clear_buffer(); return;
  }


  uint16_t rxCrc = (uint16_t(receiveBuffer[bufferIndex-2]) << 8) | receiveBuffer[bufferIndex-1];
  uint16_t calcCrc = calculate_CRC16_ccitt(receiveBuffer, bufferIndex - 2);
  if (rxCrc != calcCrc) { UART0_DEBUG_PORT.println("CRC error"); clear_buffer(); return; }


  if (proto != 0x97FB && proto != 0x96FB) {
    UART0_DEBUG_PORT.println("Ошибка: неверный протокол"); 
  }

  // UART0_DEBUG_PORT.print("Сообщение в HEX: ");
  // for (int i = 0; i < bufferIndex; i++) {
  //   if (receiveBuffer[i] < 0x10) UART0_DEBUG_PORT.print("0");
  //   UART0_DEBUG_PORT.print(receiveBuffer[i], HEX);
  //   UART0_DEBUG_PORT.print(" ");
  // }
  // UART0_DEBUG_PORT.println();

  // TLV-парсинг
  const int appStart = 5;                
  const int appEnd   = bufferIndex - 2;  // до CRC

  String msgName = "";
  long operation_Number = -1;
  long amount = -1;

  for (int p = appStart; p < appEnd; ) {
    uint8_t tag = receiveBuffer[p++];

   
    if (p >= appEnd) break;
    uint8_t len = receiveBuffer[p++];

    if (p + len > appEnd) { UART0_DEBUG_PORT.println("TLV выходит за границы"); break; }

    switch (tag) {
      case 0x01: { 
        msgName = "";
        for (int i=0;i<len;i++) msgName += char(receiveBuffer[p+i]);
      } break;

      case 0x03: { 
        String s=""; for (int i=0;i<len;i++) s += char(receiveBuffer[p+i]);
        operation_Number = s.toInt();
        operationNumber = operation_Number;
      } break;

      case 0x04: { 
        String s=""; for (int i=0;i<len;i++) s += char(receiveBuffer[p+i]);
        amount = s.toInt();
      } break;


    }
    p += len;
  }


  if (receivedTLV.isMesProcessed){
    receivedTLV.amount = amount;
    receivedTLV.mesName = msgName;
    receivedTLV.opNumber = operationNumber;
    receivedTLV.isMesProcessed = false;
    receivedTLV.lastTime = millis();
  }

//  UART0_DEBUG_PORT.print("Имя: "); UART0_DEBUG_PORT.println(msgName);
  // UART0_DEBUG_PORT.print("Имя_TLV: "); UART0_DEBUG_PORT.println(receivedTLV.mesName);
  // if (amount >= 0) {
  //   UART0_DEBUG_PORT.print("Сумма: "); 
  //   UART0_DEBUG_PORT.print(amount/100); UART0_DEBUG_PORT.print(" руб "); 
  //   UART0_DEBUG_PORT.print(amount%100); UART0_DEBUG_PORT.println(" коп");
  // }
  // if (operation_Number >= 0) {
  //   UART0_DEBUG_PORT.print("номер операции: "); UART0_DEBUG_PORT.println(get_current_operation_number());
  // }

  clear_buffer();
}


// Обработка успешного платежа
void handle_successful_payment() {
    send_IDL();
    stayIDLE = false;
    UART0_DEBUG_PORT.println("Оплата успешно проведена");
    payment.paidTimeUTC = getISO8601Time();
    payment.paymentStatus = PAID;
    payment.paidMinor = receivedTLV.amount;
    payment.kWattPerHourAvailable += payment.paidMinor / PRICE_FOR_ONE_KWHOUR;  //возможно провести несколько оплат в рамках одной сессии зарядки
    //   //ВАЖНО!!!
      //нужно хранить номера операции и суммы для каждой оплаты, чтобы при возврате была возможность вернуть сдачу с предыдущих платежей
      //наприемер оплата1: 100 руб.; оплата2: 100 руб.; сумма 200 руб. расход 50 руб. - на возврат 150 руб., 
      //вернуть 150 руб. из оплата1 или оплата2 не получиться, так как сумма возврата больше,
      //тогда надо вернуть оплата2 полностью 100 руб. и платеж1 вернуть 50 руб. - именно в такой последовательности
}

// Обработка ошибки платежа
void handle_failed_payment() {
    UART0_DEBUG_PORT.println("Ошибка при проведении оплаты");
    delay(2000);
    send_IDL();
}

// Обработка таймаута
void handle_payment_timeout() {
    UART0_DEBUG_PORT.println("Превышено время ожидания оплаты");
    delay(1000);
    send_IDL();
}

/*
В этой функции происходит обработка различных сценариев после прохождения платежа:
1) Оплата произведена и нужно начать непосредственно зарядку путем замыкания и размыкания реле провода питания NFC считывателя дабы сымитировать прикладывание карты с бесконечным балансом
2) Зарядка идет успешно, но оплаченных денег не хватает и нужно прервать зарядку путем замыкания и размыкания реле провода питания NFC считывателя
3) Зарядку прервали, но деньги доступные еще есть и соответственно нужно сделать возврат средств
*/
void charging_managment(){
    #define REFUNDPERIOD 15000
    static unsigned long debugTime = millis();
    static unsigned long debugRefundTime = millis();

    if (0) {//millis() - debugTime > 3000){  
      UART0_DEBUG_PORT.print("Статус зарядки = ");
      UART0_DEBUG_PORT.print(payment.chargingStatus);    
      UART0_DEBUG_PORT.print(" :   Текущая мощность, Вт: ");
      UART0_DEBUG_PORT.print(read_power());
      UART0_DEBUG_PORT.print(";  Текущее напряжение, В: ");
      UART0_DEBUG_PORT.print(read_voltage());
      UART0_DEBUG_PORT.print(";  Текущий ток, А: ");
      UART0_DEBUG_PORT.print(read_current());
      UART0_DEBUG_PORT.print(";  Энергии потрачено = ");
      UART0_DEBUG_PORT.print(read_total_energy());
      UART0_DEBUG_PORT.print(" кВтч из = ");
      UART0_DEBUG_PORT.print(payment.kWattPerHourAvailable);
      UART0_DEBUG_PORT.println(" кВтч");

      UART0_DEBUG_PORT.print("Статус оплаты = ");
      UART0_DEBUG_PORT.print(payment.paymentStatus);
      UART0_DEBUG_PORT.print("  Потрачено: ");
      UART0_DEBUG_PORT.print(read_total_energy()*PRICE_FOR_ONE_KWHOUR);
      UART0_DEBUG_PORT.print("  коп. из: ");
      UART0_DEBUG_PORT.print(payment.paidMinor);
      UART0_DEBUG_PORT.print(" коп.; на возврат: ");
      UART0_DEBUG_PORT.print(refundAmount);
      UART0_DEBUG_PORT.println(" коп.");
      debugTime = millis();
    }  

    if ((refundAmount) && (millis() - debugRefundTime > REFUNDPERIOD)){
      send_FIN(amountFIN, opNumberPrev); //финализация расхода средств для возврата остатка
      UART0_DEBUG_PORT.println("Возврат средств и сброс оплаты");
      payment.kWattPerHourAvailable = 0.0;
      //softserial_energy_port_send_command("E");   
      reset_energy_counter();
      //softserial_energy_port_send_command("R OFF");                                                       //Отключаем NFC карту, ожидание средства возвращены    
      digitalWrite(RELAY_PIN, LOW); // включить реле
      amountFIN = 0.0;
      refundAmount = 0.0;
      payment.paidMinor = 0.0; 
      
      payment.paymentStatus = WAITING_PAYMENT;    // или NOT_PAID, если так задумано
      payment.chargingStatus = WAITING_TO_CHARGE; // ждём старта/новой сессии

      debugRefundTime = millis();
    }

    if((payment.paymentStatus == PAID) && (payment.kWattPerHourAvailable > 0)){
      payment.chargingStatus = START_TO_CHARGE;      
      payment.paymentStatus = WAITING_PAYMENT;
    }else if((payment.chargingStatus == START_TO_CHARGE) && (read_power() > 3000) && (payment.kWattPerHourAvailable > read_total_energy())){
      payment.chargingStatus = RUNNING; 
      payment.paymentStatus = SPENDING;
    }else if((payment.chargingStatus == RUNNING) && (read_power() >= 3000) && (payment.kWattPerHourAvailable <= read_total_energy())){
      payment.chargingStatus = doSTOP;
      payment.paymentStatus = INSUFFICIENT_FUNDS;
    }else if((read_power() < 3000) && (millis() - debugRefundTime > 30000) && (payment.kWattPerHourAvailable > read_total_energy()) && (payment.paymentStatus == SPENDING)){
      payment.chargingStatus = STOPPING;
      payment.paymentStatus = REFUND;
      debugRefundTime = millis();
    }else if((read_power() < 3000) && (millis() - debugRefundTime > 3000) && (payment.kWattPerHourAvailable < read_total_energy())){
      payment.chargingStatus = WAITING_TO_CHARGE;
    }

    if((payment.chargingStatus != payment.chargingStatusPrev)){
      switch (payment.chargingStatus)
      {
      case WAITING_TO_CHARGE:
        digitalWrite(RELAY_PIN, HIGH);
        break;
      case START_TO_CHARGE:
        reset_energy_counter();
        UART0_DEBUG_PORT.println("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
        UART0_DEBUG_PORT.println("Начало сессии: включаем реле, сброс счетчика электроэнергии"); 
        UART0_DEBUG_PORT.print("Оплачено: ");
        UART0_DEBUG_PORT.print(payment.paidMinor);
        UART0_DEBUG_PORT.print(" коп. или ");
        UART0_DEBUG_PORT.print(payment.kWattPerHourAvailable);
        UART0_DEBUG_PORT.print(" кВтч по тарифу:");
        UART0_DEBUG_PORT.print(PRICE_FOR_ONE_KWHOUR);
        UART0_DEBUG_PORT.println(" коп./кВтч ");
        digitalWrite(RELAY_PIN, HIGH);
        break;
      case RUNNING:
        UART0_DEBUG_PORT.println("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
        UART0_DEBUG_PORT.println("Потребление энергии началось. Отключаем NFC карту");
        UART0_DEBUG_PORT.print("Мощность зарядки, Вт: ");
        UART0_DEBUG_PORT.println(read_power());
        digitalWrite(RELAY_PIN, LOW);
        break;

      case STOPPING:
        UART0_DEBUG_PORT.println("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
        UART0_DEBUG_PORT.print("Зарядка остановилась. Мощность, Вт: ");
        UART0_DEBUG_PORT.print(read_power());
        //softserial_energy_port_send_command("R ON");                                                              //Имитация прикладывания карты
        digitalWrite(RELAY_PIN, HIGH);  // вкл
        UART0_DEBUG_PORT.println("реле включено для оплаты NFC");                                                 //для оплаты зарядки с NFC карты

        break;   
      case doSTOP:
        UART0_DEBUG_PORT.println("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -");
        UART0_DEBUG_PORT.println("СТОП! остановка зарядки"); 
        //softserial_energy_port_send_command("R ON");                                                              //Имитация прикладывания карты
        digitalWrite(RELAY_PIN, HIGH);
        UART0_DEBUG_PORT.println("реле включено для оплаты NFC");                                              //для останова процесса зарядки

        break; 
      default:
        break;
      }


      UART0_DEBUG_PORT.print("Статус зарядки = ");
      UART0_DEBUG_PORT.print(payment.chargingStatus);
      UART0_DEBUG_PORT.print(";  Текущая мощность, Вт: ");
      UART0_DEBUG_PORT.print(read_power());
      UART0_DEBUG_PORT.print(";  Текущее напряжение, В: ");
      UART0_DEBUG_PORT.print(read_voltage());
      UART0_DEBUG_PORT.print(";  Текущий ток, А: ");
      UART0_DEBUG_PORT.print(read_current());
      UART0_DEBUG_PORT.print(";  Энергии потрачено = ");
      UART0_DEBUG_PORT.print(read_total_energy());
      UART0_DEBUG_PORT.print(" кВтч из = ");
      UART0_DEBUG_PORT.print(payment.kWattPerHourAvailable);
      UART0_DEBUG_PORT.println(" кВтч");
      

      payment.chargingStatusPrev = payment.chargingStatus;
    }


    if((payment.paymentStatus != payment.paymentStatusPrev)){

      switch (payment.paymentStatus)
      {
      case PAID:
        break;

      case WAITING_PAYMENT:
     
        break;

      case SPENDING:
        send_DIS();
        break;

      case INSUFFICIENT_FUNDS:
        UART0_DEBUG_PORT.print("Было оплачено: ");
        UART0_DEBUG_PORT.print(payment.paidMinor);
        UART0_DEBUG_PORT.print(" и доступно "); 
        UART0_DEBUG_PORT.print(payment.kWattPerHourAvailable);
        UART0_DEBUG_PORT.print("кВтч; Было израсходовано = ");
        UART0_DEBUG_PORT.print(read_total_energy());
        UART0_DEBUG_PORT.println(" кВтч");
        send_POST_json(payment.paidTimeUTC, payment.paidMinor, 0, read_total_energy());
        payment.kWattPerHourAvailable = 0.0;                                                                    //сброс оплаты так как средства закончились
        payment.paidMinor = 0.0;
        //softserial_energy_port_send_command("E");                                                               // Обнулить счетчик электроэнергии на Arduino    
        reset_energy_counter();
        stayIDLE = true;
        
        break;

      case REFUND:
        send_IDL();
        UART0_DEBUG_PORT.println("<  >  <  >  <  >  <  >  <  >  <  >  <  >  <  >  <  >");
        UART0_DEBUG_PORT.print("Будет выполнен возврат средств. Остаток: ");
        
        amountFIN = read_total_energy()*PRICE_FOR_ONE_KWHOUR; //в копейках
        opNumberPrev = receivedTLV.opNumber;
        refundAmount = long(payment.paidMinor) - amountFIN;
        UART0_DEBUG_PORT.print(refundAmount);
        UART0_DEBUG_PORT.println(" коп.");
        UART0_DEBUG_PORT.print("  Было оплачено: ");
        UART0_DEBUG_PORT.print(payment.paidMinor);
        UART0_DEBUG_PORT.print(" коп.; израсходовано: ");
        UART0_DEBUG_PORT.println(amountFIN);
        send_POST_json(payment.paidTimeUTC, payment.paidMinor, refundAmount, read_total_energy());
        debugRefundTime = millis();
        reset_energy_counter();
        break;
      
      default:
        break;
      }

      UART0_DEBUG_PORT.print("-- Статус оплаты = ");
      UART0_DEBUG_PORT.print(payment.paymentStatus);
      UART0_DEBUG_PORT.print("; -- Потрачено: ");
      UART0_DEBUG_PORT.print(amountFIN);
      UART0_DEBUG_PORT.print(" коп. из: ");
      UART0_DEBUG_PORT.print(payment.paidMinor);
      UART0_DEBUG_PORT.print(" коп.; -- на возврат: ");
      UART0_DEBUG_PORT.print(refundAmount);
      UART0_DEBUG_PORT.println(" коп. --");
      

      payment.paymentStatusPrev = payment.paymentStatus;
    }


    



    //if(payment.paymentStatus == NOT_PAID) return;


    // if(payment.isChargingActive == INACTIVE){                                                                 // Платеж прошел
    //   softserial_energy_port_send_command("E");                                                               // Обнулить счетсчик электроэнергии на Arduino    
    //   softserial_energy_port_send_command("R ON");                                                            // Так как оплата прошла успешно, то включить реле
    //   UART0_DEBUG_PORT.println("Сбросить счетсчик электроэнергии");
    //   payment.isChargingActive = ACTIVE;
    // }else if((read_power() > 300) && (payment.isChargingActive == ACTIVE) && (payment.kWattPerHourAvailable > read_total_energy())){ 
    //   ///////////Обработка ситуации когда началось потребление энергии и нужна имитация того что убрали карту //
    //   softserial_energy_port_send_command("R OFF");

    //   UART0_DEBUG_PORT.println("Потребление энергии началось, убрать карту");
    // }else if((read_power() < 300) && (payment.isChargingActive == ACTIVE) && (payment.kWattPerHourAvailable > read_total_energy())){
    //   ///////////Обработка ситуации когда кабель вытащили из авто и нужно сделать возврат средств
    //    //sendREFUND(long amount, int operationNumber); 
    //    UART0_DEBUG_PORT.println("Возврат средств");
    //    payment.isPaymentSucsess = NOT_PAID;

    // }else if((read_power() > 300) && (payment.isChargingActive == ACTIVE) && (payment.kWattPerHourAvailable < read_total_energy())){  //read_total_energy() это функция которая возвращает сколько энергии было потрачено
    //   //////////Обработка ситуации когда израсходованы кв.ч и нужно прервать зарядку путем имитации прикладывания карты
    //   UART0_DEBUG_PORT.println("Средства закончились. Было доступно= "); 
    //   UART0_DEBUG_PORT.print(payment.kWattPerHourAvailable);
    //   UART0_DEBUG_PORT.print(" Было потрачено= ");
    //   UART0_DEBUG_PORT.println(read_total_energy());

    //   payment.isPaymentSucsess == NOT_PAID;
    //   payment.isChargingActive = INACTIVE;
    //   softserial_energy_port_send_command("R ON");                                                             //Имитация прикладывания карты
    //   delay(1000);
    //   softserial_energy_port_send_command("R OFF");

    // }
} 