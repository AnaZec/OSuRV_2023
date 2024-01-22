#include <stdio.h>
#ifdef __linux
  #include <unistd.h>
#else
  #include <windows.h>
#endif

#include "libenjoy.h"


#include <stdint.h> // uint16_t and family
#include <stdbool.h> // bool
#include <stdio.h> // printf and family
#include <string.h> // strerror()
#include <unistd.h> // file ops
#include <fcntl.h> // open() flags
#include <sys/ioctl.h> // ioctl()
#include <errno.h> //errno

#include "/home/pi/OSuRV_2023/SW/Driver/motor_ctrl/include/motor_ctrl.h"// dev/motor_ctrl

static int16_t duty[2]; //duty[0] - BLDC, duty[1] - servo


//Funkcija za konvertovanje brojeva joypad-a u duty kod servomotora
int joypad2duty(int joyNum){

	// Provera da li je vrednost unutar opsega
	if (joyNum < -32767 || joyNum > 32767) {
		fprintf(stderr, "Vrednost van opsega (-32767 do 32767)\n");
		return -1; // Indikacija greške
	}

	// Linearno preslikavanje
	int novo_min = 350;
	int novo_max = 650;
	int staro_min = -32767;
	int staro_max = 32767;

	int duty_servo = (joyNum - staro_min) * (novo_max - novo_min) / (staro_max - staro_min) + novo_min;
	return duty_servo;

}

//Funkija za konvertovanje brojeva joypada-a u speed kod bldc motora
int joypad2speed(int joyNum){

	// Provera da li je vrednost unutar opsega
	if (joyNum < -32767 || joyNum > 32767) {
		fprintf(stderr, "Vrednost van opsega (-32767 do 0)\n");
		return -1; // Indikacija greške
	}

	// Linearno preslikavanje
	int novo_min = -100;
	int novo_max = 100;
	int staro_min = 32767;
	int staro_max = -32767;

	int speed = (joyNum - staro_min) * (novo_max - novo_min) / (staro_max - staro_min) + novo_min;
	return speed;

}


//Funkcija run_bldc
int runBLDC(int speed){ //BLDC je channel 0 ostali su servo
	

	// |speed| = [0, 100] -> threshold = [10, 0].
	// it will be <<1 in write() so then will be [20, 0].
	uint8_t abs_speed;
	uint16_t threshold;
	if(speed >= 0){
		abs_speed = speed;
		threshold = (100-abs_speed)/10;
		duty[0] = threshold;
	}else{
		abs_speed = -speed;
		threshold = (100-abs_speed)/10;
		duty[0] = -threshold;
	}
	
	for(int i = 0; i < 2; i++){
		printf("duty[%d] = %d\n",i, duty[i]);
	}
	
	printf("speed = %d\n", speed);
		
	return 0;

}

//Funkcija za upravljacki servo za skretanje
int runServo(int duty_servo){


	duty[1] = duty_servo;

	for(int i = 0; i < 2; i++){
		printf("duty[%d] = %d\n",i, duty[i]);
	}

	return 0;

}


// This tels msvc to link agains winmm.lib. Pretty nasty though.
#pragma comment(lib, "winmm.lib")

int main()
{

    //Faktori ispune
    int speed;
    int duty_servo;
	
    //flags
    int flagBLDC;
    int flagServo;
    
    int r,s;
	    
    //Otvoriti fd.
	    
    int fd;
    fd = open(DEV_FN, O_RDWR); //dev/motor_ctrl
    if(fd < 0){

	fprintf(stderr, "ERROR: \"%s\" not opened!\n", DEV_FN);
	fprintf(stderr, "fd = %d %s\n", fd, strerror(-fd));
	return 4;
    }
    
    const uint16_t moduo = 20; // 5kHz
 
    motor_ctrl__ioctl_arg_moduo_t ia;
    ia.ch = 0;
    ia.moduo = moduo;
    
    r = ioctl(fd, IOCTL_MOTOR_CLTR_SET_MODUO, *(unsigned long*)&ia);
    if(r){
    
	return 3; //ioctl went wrong returning
	
    }

     
    libenjoy_context *ctx = libenjoy_init(); // initialize the library
    libenjoy_joy_info_list *info;

    libenjoy_enumerate(ctx);

    info = libenjoy_get_info_list(ctx);

    if(info->count != 0) // just get the first joystick
    {
        libenjoy_joystick *joy;
        printf("Opening joystick %s...", info->list[0]->name);
 
        joy = libenjoy_open_joystick(ctx, info->list[0]->id);
        if(joy)
        {
            int counter = 0;
            libenjoy_event ev;

            printf("Success!\n");
            printf("Axes: %d btns: %d\n", libenjoy_get_axes_num(joy),libenjoy_get_buttons_num(joy));
        
            while(1)
            {
                // Value data are not stored in library. if you want to use
                // them, you have to store them

                // That's right, only polling available
                while(libenjoy_poll(ctx, &ev))
                {
                    switch(ev.type)
                    {
                    case LIBENJOY_EV_AXIS:
                        printf("%u: axis %d val %d\n", ev.joy_id, ev.part_id, ev.data);

                        if(ev.part_id == 1){ //axis 1 - y-osa

                            speed = joypad2speed(ev.data);
                            
                            //poziva se BLDC motor (pogon) - runBLDC(povratna vrednost od joypad2speed)
			    flagBLDC = runBLDC(speed);

			    lseek(fd, SEEK_SET, 0); // seek on start
	
			    s = sizeof(duty[0])*1;
			    r = write(fd, (char*)&duty[0], s);
			    printf("s = %d r = %d\n", s,r); // dodato za debagovanje
			    if(r != s){
				perror("write went wrong\n");
				return 4; //write went wrong
						
	           	    }
				
                        }else if(ev.part_id == 0){ //axis 0 - x-osa

                            duty_servo = joypad2duty(ev.data);

                            //poziva se servo motor za skretanje levo - desno
			    flagServo = runServo(duty_servo);
			    

			    lseek(fd, SEEK_SET, 0); // seek on start
				
			    s = sizeof(duty[1])*1;
			    r = write(fd, (char*)&duty[1], s);
			    printf("s = %d r = %d\n", s,r);
			    if(r != s){
				perror("write went wrong\n");
				return 4; //write went wrong
						
			    }
	
                        }


                        break;
                    case LIBENJOY_EV_BUTTON:
                        printf("%u: button %d val %d\n", ev.joy_id, ev.part_id, ev.data);
                        break;
                    case LIBENJOY_EV_CONNECTED:
                        printf("%u: status changed: %d\n", ev.joy_id, ev.data);
                        break;
                    }
                }
#ifdef __linux
                usleep(50000);
#else
                Sleep(50);
#endif
                counter += 50;
                if(counter >= 1000)
                {
                    libenjoy_enumerate(ctx);
                    counter = 0;
                }
            }

	    //Zatvoriti fd
		
	    lseek(fd, SEEK_SET, 0); // seek on start

	    close(fd);

            libenjoy_close_joystick(joy);

	    printf("End.\n");
        }
        else
            printf("Failed!\n");
    }
    else
        printf("No joystick available\n");

    // Frees memory allocated by that joystick list. Do not forget it!
    libenjoy_free_info_list(info);

    // deallocates all memory used by lib. Do not forget this!
    // libenjoy_poll must not be called during or after this call
    libenjoy_close(ctx);
    return 0;
}
