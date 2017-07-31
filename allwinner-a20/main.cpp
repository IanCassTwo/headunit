#include <glib.h>
#include <stdio.h>
#define GDK_VERSION_MIN_REQUIRED (GDK_VERSION_3_10)
#include <gdk/gdk.h>
#include <time.h>

//This gets defined by SDL and breaks the protobuf headers
#undef Status

#include "hu_uti.h"
#include "hu_aap.h"

#include "gps/gps.h"

#include "main.h"
#include "outputs.h"
#include "callbacks.h"

gst_app_t gst_app;

float g_dpi_scalefactor = 1.0f;

IHUAnyThreadInterface* g_hu = nullptr;

uint64_t
get_cur_timestamp() {
        struct timespec tp;
        /* Fetch the time stamp */
        clock_gettime(CLOCK_REALTIME, &tp);

        return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
}

static int
gst_loop(gst_app_t *app) {
        int ret;
        GstStateChangeReturn state_ret;

        app->loop = g_main_loop_new(NULL, FALSE);
        printf("Starting Android Auto...\n");
        g_main_loop_run(app->loop);

        return ret;
}

void gps_location_handler(uint64_t timestamp, double lat, double lng, double bearing, double speed, double alt, double accuracy) {
        logd("[LOC][%" PRIu64 "] - Lat: %f Lng: %f Brng: %f Spd: %f Alt: %f Acc: %f \n",
                        timestamp, lat, lng, bearing, speed, alt, accuracy);

        g_hu->hu_queue_command([timestamp, lat, lng, bearing, speed, alt, accuracy](IHUConnectionThreadInterface& s)
        {
                HU::SensorEvent sensorEvent;
                HU::SensorEvent::LocationData* location = sensorEvent.add_location_data();
                location->set_timestamp(timestamp);
                location->set_latitude(static_cast<int32_t>(lat * 1E7));
                location->set_longitude(static_cast<int32_t>(lng * 1E7));

                if (bearing != 0) {
                        location->set_bearing(static_cast<int32_t>(bearing * 1E6));
                }

                // AA expects speed in knots, so convert back
                location->set_speed(static_cast<int32_t>((speed / 1.852) * 1E3));

                if (alt != 0) {
                        location->set_altitude(static_cast<int32_t>(alt * 1E2));
                }

                location->set_accuracy(static_cast<int32_t>(accuracy * 1E3));

                s.hu_aap_enc_send_message(0, AA_CH_SEN, HU_SENSOR_CHANNEL_MESSAGE::SensorEvent, sensorEvent);
        });
}


int
main(int argc, char *argv[]) {

        GOOGLE_PROTOBUF_VERIFY_VERSION;

        hu_log_library_versions();
        hu_install_crash_handler();
#if defined GDK_VERSION_3_10
        printf("GTK VERSION 3.10.0 or higher\n");
        //Assuming we are on Gnome, what's the DPI scale factor?
        gdk_init(&argc, &argv);

        GdkScreen * primaryDisplay = gdk_screen_get_default();
        if (primaryDisplay) {
                g_dpi_scalefactor = (float) gdk_screen_get_monitor_scale_factor(primaryDisplay, 0);
                printf("Got gdk_screen_get_monitor_scale_factor() == %f\n", g_dpi_scalefactor);
        }
#else
        printf("Using hard coded scalefactor\n");
        g_dpi_scalefactor = 1;
#endif
        gst_app_t *app = &gst_app;
        int ret = 0;
        errno = 0;

        gst_init(NULL, NULL);
        struct sigaction action;
        sigaction(SIGINT, NULL, &action);
        SDL_Init(SDL_INIT_EVERYTHING);
        sigaction(SIGINT, &action, NULL);

        DesktopCommandServerCallbacks commandCallbacks;
        CommandServer commandServer(commandCallbacks);
        if (!commandServer.Start())
        {
            loge("Command server failed to start");
        }

        //loop to emulate the car
        while(true)
        {
            DesktopEventCallbacks callbacks;
            HUServer headunit(callbacks);

            /* Start AA processing */
            ret = headunit.hu_aap_start(HU_TRANSPORT_TYPE::USB, true);
            if (ret < 0) {
                    printf("Phone is not connected. Connect a supported phone and restart.\n");
                    return 0;
            }

            callbacks.connected = true;

            g_hu = &headunit.GetAnyThreadInterface();
            commandCallbacks.eventCallbacks = &callbacks;

            // GPS processing
            gps_start(&gps_location_handler);

            /* Start gstreamer pipeline and main loop */
            ret = gst_loop(app);
            if (ret < 0) {
                    printf("STATUS:gst_loop() ret: %d\n", ret);
            }

            callbacks.connected = false;
            commandCallbacks.eventCallbacks = nullptr;

            printf("waiting for gps_thread\n");
            gps_stop();

            /* Stop AA processing */
            ret = headunit.hu_aap_shutdown();
            if (ret < 0) {
                    printf("STATUS:hu_aap_stop() ret: %d\n", ret);
                    SDL_Quit();
                    return (ret);
            }

            g_hu = nullptr;
        }

        SDL_Quit();

        return (ret);
}
