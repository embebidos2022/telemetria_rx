#include <stdio.h>
#include "pico/stdlib.h"
#include "sd_card.h"
#include "ff.h"
#include "sd_manager.h"
#include "nrf24_driver.h"
#include "hardware/gpio.h"

//Variables para la SD
FRESULT fr;
FATFS fs;
FIL fil_acel;
FIL fil_gyro;
FIL fil_magnet;
FIL fil_angles;
int ret;
char buf[100];
char filename[] = "acelData.txt";
char filename2[] = "gyroData.txt";
char filename3[] = "magnetData.txt";
char filename4[] = "anglesData.txt";
char str[100];

//Funciones
void sys_init(void);
void sys_stop(void);

//Pines
const uint interruptPin = 22;

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

    // holds payload_zero sent by the transmitter
    uint8_t payload_zero = 0;

    // holds payload_one sent by the transmitter
    uint8_t payload_one[5];

    // two byte struct sent by transmitter
    typedef struct payload_two_s { uint8_t one; uint8_t two; } payload_two_t;

    // holds payload_two struct sent by the transmitter
    payload_two_t payload_two;

    //Archivos donde se guardaran los datos
    sd_openfileW(fr, &fil_acel, filename);
    sd_openfileW(fr, &fil_gyro, filename2);
    sd_openfileW(fr, &fil_magnet, filename3);
    sd_openfileW(fr, &fil_angles, filename4);

    while(1){
        if (my_nrf.is_packet(&pipe_number))
        {
            switch (pipe_number)
            {
                case DATA_PIPE_0:
                // read payload
                my_nrf.read_packet(&payload_zero, sizeof(payload_zero));

                // receiving a one byte uint8_t payload on DATA_PIPE_0
                printf("\nPacket received:- Payload (%d) on data pipe (%d)\n", payload_zero, pipe_number);
                break;
                
                case DATA_PIPE_1:
                // read payload
                my_nrf.read_packet(payload_one, sizeof(payload_one));

                // receiving a five byte string payload on DATA_PIPE_1
                printf("\nPacket received:- Payload (%s) on data pipe (%d)\n", payload_one, pipe_number);
                break;
                
                case DATA_PIPE_2:
                // read payload
                my_nrf.read_packet(&payload_two, sizeof(payload_two));

                // receiving a two byte struct payload on DATA_PIPE_2
                printf("\nPacket received:- Payload (1: %d, 2: %d) on data pipe (%d)\n", payload_two.one, payload_two.two, pipe_number);
                break;
                
                case DATA_PIPE_3:
                break;
                
                case DATA_PIPE_4:
                break;
                
                case DATA_PIPE_5:
                break;
                
                default:
                break;
            }
        }

        //Escribiendo datos del acelerometro
        sprintf(str, "Escribiendo en el archivo del acelerometro\n");
        sd_writefile(ret, &fil_acel, str);

        //Escribiendo datos del giroscopio
        sprintf(str, "Escribiendo en el archivo del giroscopio\n");
        sd_writefile(ret, &fil_gyro, str);

        //Escribiendo datos del magnetometro
        sprintf(str, "Escribiendo en el archivo del magnetometro\n");
        sd_writefile(ret, &fil_magnet, str);

        //Escribiendo angulos
        sprintf(str, "Escribiendo en el archivo de los angulos\n");
        sd_writefile(ret, &fil_angles, str);

        if(gpio_get(interruptPin) == 1){ //Haciendo polling al botón de parar de guardar los datos (PENDIENTE POR INTERRUPCION)
            sys_stop();
        }

        sleep_ms(100);
    }
}

void sys_init(void){
    stdio_init_all();
    gpio_init(interruptPin);
    gpio_set_dir(interruptPin, GPIO_IN);

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
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    printf("Datos escritos");
    while (1);
}