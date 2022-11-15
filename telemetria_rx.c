#include <stdio.h>
#include "pico/stdlib.h"
#include "sd_card.h"
#include "ff.h"
#include "sd_manager.h"
#include "nrf24_driver.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

//Variables para la SD
FRESULT fr;
FATFS fs;
FIL fil_acel;
FIL fil_gyro;
FIL fil_magnet;
FIL fil_angles;
FIL fil_wind;
int ret;
char buf[100];
char filename[] = "acelData.txt";
char filename2[] = "gyroData.txt";
char filename3[] = "magnetData.txt";
char filename4[] = "anglesData.txt";
char filename5[] = "windData.txt";
char str[100];

//Funciones
void sys_init(void);
void sys_stop(void);

//Pines
//const uint interruptPin = 22;

//Para controlar el servo2
bool rightButton = false;
bool leftButton = false;
const int rightPin = 22;
const int leftPin = 15;
float grados2 = 90;

//Para control de servo1
const int velaPin = 28;
uint16_t potenciometro;
float grados1;

//Factor de converion del adc
const float conversion_factor = 3.3f / (1 << 12);

bool right, left;

int main()
{
    sys_init();

    // GPIO pin numbers
    pin_manager_t my_pins = { 
        .sck = 2,
        .copi = 3, 
        .cipo = 4, 
        .csn = 5, 
        .ce = 6 
    };

    nrf_manager_t my_config = {
        // RF Channel 
        .channel = 120,

        // AW_3_BYTES, AW_4_BYTES, AW_5_BYTES
        .address_width = AW_5_BYTES,

        // dynamic payloads: DYNPD_ENABLE, DYNPD_DISABLE
        .dyn_payloads = DYNPD_ENABLE,

        // data rate: RF_DR_250KBPS, RF_DR_1MBPS, RF_DR_2MBPS
        .data_rate = RF_DR_1MBPS,

        // RF_PWR_NEG_18DBM, RF_PWR_NEG_12DBM, RF_PWR_NEG_6DBM, RF_PWR_0DBM
        .power = RF_PWR_NEG_12DBM,

        // retransmission count: ARC_NONE...ARC_15RT
        .retr_count = ARC_10RT,

        // retransmission delay: ARD_250US, ARD_500US, ARD_750US, ARD_1000US
        .retr_delay = ARD_500US 
    };

    // SPI baudrate
    uint32_t my_baudrate = 5000000;

    // provides access to driver functions
    nrf_client_t my_nrf;

    // initialise my_nrf
    nrf_driver_create_client(&my_nrf);

    // configure GPIO pins and SPI
    my_nrf.configure(&my_pins, my_baudrate);

    // not using default configuration (my_nrf.initialise(NULL)) 
    my_nrf.initialise(&my_config);

    /**
     * set addresses for DATA_PIPE_0 - DATA_PIPE_3.
     * These are addresses the transmitter will send its packets to.
     */
    my_nrf.rx_destination(DATA_PIPE_0, (uint8_t[]){0x37,0x37,0x37,0x37,0x37});
    my_nrf.rx_destination(DATA_PIPE_1, (uint8_t[]){0xC7,0xC7,0xC7,0xC7,0xC7});
    my_nrf.rx_destination(DATA_PIPE_2, (uint8_t[]){0xC8,0xC7,0xC7,0xC7,0xC7});
    my_nrf.rx_destination(DATA_PIPE_3, (uint8_t[]){0xC9,0xC7,0xC7,0xC7,0xC7});

    // set to RX Mode
    my_nrf.receiver_mode();

    // data pipe number a packet was received on
    uint8_t pipe_number = 0;

    typedef struct payload2Send_s{
        uint16_t velaDegree;
        bool right;
        bool left; 
    } payload2Send_t;
  
    typedef struct payload2Receive_s{
        bool request;
        int16_t Acel[3];
        int16_t Gyro[3];
        int16_t Magneto[3];
        int16_t WindDir;
        float WindSpeed;
    } payload2Receive_t;

    payload2Receive_t payload2Receive;

    //Archivos donde se guardaran los datos
    sd_openfileW(fr, &fil_acel, filename);
    sd_openfileW(fr, &fil_gyro, filename2);
    sd_openfileW(fr, &fil_magnet, filename3);
    sd_openfileW(fr, &fil_angles, filename4);
    sd_openfileW(fr, &fil_wind, filename5);

    uint64_t time_sent = 0; // time packet was sent
    uint64_t time_reply = 0; // response time after packet sent

    // result of packet transmission
    fn_status_t success = 0;

    //gpio_put(PICO_DEFAULT_LED_PIN, 1);

    while(1){
        my_nrf.receiver_mode();
        right = 0;
        left = 0;
        if (my_nrf.is_packet(&pipe_number))
        {
            switch (pipe_number)
            {
                case DATA_PIPE_0:
                    // read payload
                    my_nrf.read_packet(&payload2Receive, sizeof(payload2Receive));

                    // receiving a two byte struct payload on DATA_PIPE_2
                    //printf("\nPacket received:- Payload %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %f on data pipe (%d)\n", payload2Receive.request,
                    //payload2Receive.Acel[0], payload2Receive.Acel[1], payload2Receive.Acel[2], payload2Receive.Gyro[0], payload2Receive.Gyro[1], payload2Receive.Gyro[2],
                    //payload2Receive.Magneto[0], payload2Receive.Magneto[1], payload2Receive.Magneto[2], payload2Receive.WindDir, payload2Receive.WindSpeed, pipe_number);
                    //Escribiendo angulos
                    //sprintf(str, "%d,%d\n", payload_two.angle2);
                    //sd_writefile(ret, &fil_angles, str);
                    
                    my_nrf.standby_mode();
                    //printf("Preparando para enviar control\r\n");

                    //while(true){

                        if(gpio_get(rightPin)){
                            right = 1;
                        }
                        if(gpio_get(leftPin)){
                            left = 1;
                        }

                        potenciometro = adc_read();
                        grados1 = (potenciometro * conversion_factor) * (64);
                        
                        payload2Send_t payload2Send = {
                            .velaDegree = grados1,
                            .right = right,
                            .left = left
                        };

                        my_nrf.tx_destination((uint8_t[]){0x37,0x37,0x37,0x37,0x37});

                        // time packet was sent
                        time_sent = to_us_since_boot(get_absolute_time()); // time sent

                        // send packet to receiver's DATA_PIPE_0 address
                        success = my_nrf.send_packet(&payload2Send, sizeof(payload2Send));
                        
                        // time auto-acknowledge was received
                        time_reply = to_us_since_boot(get_absolute_time()); // response time

                        if (success)
                        {
                            printf("\nPacket sent:- Response: %lluμS | Payload: %d, %d, %d\n",time_reply - time_sent, payload2Send.velaDegree,
                                payload2Send.right, payload2Send.left);

                        }
                        else{
                            printf("\nPacket not sent:- Receiver not available.\n");
                        }

                        //Comprobando si el request es de telemetria
                        if(payload2Receive.request){
                            //Escribir los datos recibidos en la SD
                            //Escribiendo datos del acelerometro
                            sprintf(str, "%d,%d,%d\n", payload2Receive.Acel[0], payload2Receive.Acel[1], payload2Receive.Acel[2]);
                            sd_writefile(ret, &fil_acel, str);

                            //Escribiendo datos del giroscopio
                            sprintf(str, "%d,%d,%d\n", payload2Receive.Gyro[0], payload2Receive.Gyro[1], payload2Receive.Gyro[2]);
                            sd_writefile(ret, &fil_gyro, str);

                            //Escribiendo datos del magnetometro
                            sprintf(str, "%d,%d,%d\n", payload2Receive.Magneto[0], payload2Receive.Magneto[1], payload2Receive.Magneto[2]);
                            sd_writefile(ret, &fil_magnet, str);

                            //Escribiendo datos del sensor de viento
                            sprintf(str, "%d,%d\n", payload2Receive.WindDir, payload2Receive.WindSpeed);
                            sd_writefile(ret, &fil_wind, str);
                        }
                    
                        //sleep_ms(200);
                    //}
                break;
                
                default:
                break;
            }
        }
        
        /*
        if(gpio_get(interruptPin) == 1){ //Haciendo polling al botón de parar de guardar los datos (PENDIENTE POR INTERRUPCION)
            sys_stop();
        }
        */

        //sleep_ms(80);
    }
}

void sys_init(void){
    adc_init();
    stdio_init_all();
    //gpio_init(interruptPin);
    //gpio_set_dir(interruptPin, GPIO_IN);

    //Potenciometro
    adc_gpio_init(velaPin);
    adc_select_input(2);

    //Pulsadores
    gpio_init(rightPin);
    gpio_init(leftPin);
    gpio_set_dir(rightPin, GPIO_IN);
    gpio_set_dir(leftPin, GPIO_IN); 

    sleep_ms(6000);
    //Inicializar todo el apartado de la SD
    initialize_sd();
    mount_drive(fr, &fs);
}

void sys_stop(void){
    sd_closefile(fr, &fil_acel);
    sd_closefile(fr, &fil_gyro);
    sd_closefile(fr, &fil_magnet);
    sd_closefile(fr, &fil_angles);
    sd_closefile(fr, &fil_wind);
    //gpio_put(PICO_DEFAULT_LED_PIN, 1);
    printf("Datos escritos");
    while (1);
}
