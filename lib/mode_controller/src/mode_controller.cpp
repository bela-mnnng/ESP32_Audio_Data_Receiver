#include "mode_controller.h"
#include "bt_sink.h"
#include "wifi_manager.h"
#include "spotify_connect.h"
#include "mode_fsm.h"
#include "audio_output.h"

void mode_controller_init() {
    if(audio_output_init == true && bt_sink_init == true && wifi_manager_init == true && spotify_connect_init == true){
        bt_sink_start();
        wifi_manager_start();
    }else{
        mode_fsm = -1;    
        return;
}
    
    


