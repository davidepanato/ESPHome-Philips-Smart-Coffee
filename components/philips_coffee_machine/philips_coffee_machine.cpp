#include "esphome/core/log.h"
#include "philips_coffee_machine.h"

#define MAINBOARD_BUFFER_SIZE size_t(19)
#define DISPLAY_BUFFER_SIZE size_t(12)

namespace esphome
{
    namespace philips_coffee_machine
    {

        static const char *TAG = "philips_coffee_machine";

        void PhilipsCoffeeMachine::setup()
        {
            power_pin_->setup();
            power_pin_->pin_mode(gpio::FLAG_OUTPUT);
            power_pin_->digital_write(initial_pin_state_);
        }

        void PhilipsCoffeeMachine::loop()
        {
            uint8_t display_buffer[DISPLAY_BUFFER_SIZE];
            uint8_t mainboard_buffer[MAINBOARD_BUFFER_SIZE];

            // Pipe display to mainboard
            if (display_uart_.available())
            {
                uint8_t size = std::min(size_t(display_uart_.available()), DISPLAY_BUFFER_SIZE);
                display_uart_.read_array(display_buffer, size);

                // Check if a action button is currently performing a long press
                bool long_pressing = false;
#ifdef USE_BUTTON
                for (philips_action_button::ActionButton *button : action_buttons_)
                {
                    if (button->is_long_pressing())
                    {
                        long_pressing = true;
                        break;
                    }
                }
#endif

                // Drop messages if button long-press is currently injecting messages
                if (!long_pressing)
                    mainboard_uart_.write_array(display_buffer, size);
                last_message_from_display_time_ = millis();
            }

            // Read and forward until valid start bytes have been received
            while (mainboard_uart_.available())
            {
                mainboard_buffer[0] = mainboard_uart_.read();
                display_uart_.write(mainboard_buffer[0]);
                if (mainboard_buffer[0] == message_header[0])
                {
                    mainboard_buffer[1] = mainboard_uart_.read();
                    display_uart_.write(mainboard_buffer[1]);
                    if (mainboard_buffer[1] == message_header[1])
                    {
                        break;
                    }
                }
            }

            // Pipe to display
            if (mainboard_uart_.available())
            {
                uint8_t size = std::min(size_t(mainboard_uart_.available()), MAINBOARD_BUFFER_SIZE - 2);
                mainboard_uart_.read_array(mainboard_buffer + 2, size);

                display_uart_.write_array(mainboard_buffer + 2, size);

                if (size >= MAINBOARD_BUFFER_SIZE - 2)
                {
                    // Only process messages starting with start bytes and carrying a valid checksum
                    if (mainboard_buffer[0] == message_header[0] &&
                        mainboard_buffer[1] == message_header[1] &&
                        verify_checksum(mainboard_buffer, MAINBOARD_BUFFER_SIZE))
                    {
                        last_message_from_mainboard_time_ = millis();
#ifdef USE_TEXT_SENSOR
                        // Update status sensors
                        for (philips_status_sensor::StatusSensor *status_sensor : status_sensors_)
                            status_sensor->update_status(mainboard_buffer);

#ifdef USE_NUMBER
                        // Update beverage settings
                        for (philips_beverage_setting::BeverageSetting *beverage_setting : beverage_settings_)
                            beverage_setting->update_status(mainboard_buffer);
#endif
#endif
                    }
                }
            }

            // Publish power state if required as long as the display is requesting messages
            if (millis() - last_message_from_display_time_ > POWER_STATE_TIMEOUT)
            {
#ifdef USE_SWITCH
                // Update power switches
                for (philips_power_switch::Power *power_switch : power_switches_)
                    power_switch->update_state(false);
#endif

#ifdef USE_TEXT_SENSOR
                // Update status sensors
                for (philips_status_sensor::StatusSensor *status_sensor : status_sensors_)
                    status_sensor->set_state_off();
#endif
            }
            else
            {
#ifdef USE_SWITCH
                // Update power switches
                for (philips_power_switch::Power *power_switch : power_switches_)
                    power_switch->update_state(true);
#endif
            }

            display_uart_.flush();
            mainboard_uart_.flush();
        }

        void PhilipsCoffeeMachine::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Philips Coffee Machine");
        }

    } // namespace philips_coffee_machine
} // namespace esphome