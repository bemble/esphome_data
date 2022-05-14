#include "esphome.h"

#define DWIND_TAG "davis_wind"
#define DWIND_MAX_VANE_VALUE 4095

#define DWIND_OFFSET 0
#define DWIND_DIRECTION_PIN 34
// Precision, 8 or 16
// 8, direction can be: "N", "NE", "E", "SE", "S", "SW", "W", "NW"
// 16, direction can be: "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
#define DWIND_DIRECTION_PRECISION 8

/**
 * Handle Davis Instruments Vantage Pro anemometer and vane.
 * Wiring:
 *  - yellow: +5v
 *  - red: GND
 *  - green: vane
 *  - black: anemometer (pulled up with a 47K resistor)
 */
class DavisWindSensor : public PollingComponent, public Sensor
{
private:
  int vane_value;
  int degrees_direction;
  int previous_direction = -1000;

  void read_direction() {
    this->vane_value = analogRead(DWIND_DIRECTION_PIN);
    this->degrees_direction = map(this->vane_value, 0, DWIND_MAX_VANE_VALUE, 0, 360) + DWIND_OFFSET;

    if(this->degrees_direction > 360)
      this->degrees_direction -= 360;

    if(this->degrees_direction < 0)
      this->degrees_direction += 360;
  }

  std::string getHeading() {
    std::string headings_8[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    std::string headings_16[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

    float increment = 360/DWIND_DIRECTION_PRECISION;
    for(int i = 0; i < DWIND_DIRECTION_PRECISION; i++) {
      if(this->degrees_direction < (i+0.5) * increment) {
        return (DWIND_DIRECTION_PRECISION == 8) ? headings_8[i] : headings_16[i];
      }
    }

    return "N";
  }

  void publish_states()
  {
    this->vane_value_sensor->publish_state(this->vane_value);
    this->direction_in_degrees->publish_state(this->degrees_direction);
    this->heading_sensor->publish_state(this->getHeading());
  }
public:
  Sensor *vane_value_sensor = new Sensor();
  Sensor *direction_in_degrees = new Sensor();
  TextSensor *heading_sensor = new TextSensor();

  DavisWindSensor() : PollingComponent(500) {}

  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  void setup() override
  {
  }

  void update() override
  {
    this->read_direction();
    int position_difference = abs(this->degrees_direction - this->previous_direction);
    int max_diff = (DWIND_DIRECTION_PRECISION == 8) ? 10 : 5;
    if (position_difference > max_diff)
    {
      this->publish_states();
      this->previous_direction = this->degrees_direction;
    }
  }
};