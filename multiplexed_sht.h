#include "esphome.h"
#include "ClosedCube_SHT31D.h"
#include "PCF8574.h"

#define SHT_COUNT 3
#define U1 2
#define U2 1
#define U3 3

#define I2C_SHT 0x44

float compute_average(float a, float b, float c)
{
  float diffab = abs(a - b);
  float diffac = abs(a - c);
  float diffbc = abs(b - c);

  if (diffab <= diffac && diffab <= diffbc)
  {
    return (a + b) / 2;
  }
  else if (diffac <= diffab && diffac <= diffbc)
  {
    return (a + c) / 2;
  }

  return (b + c) / 2;
}

class MultiplexedShtSensor : public PollingComponent, public Sensor
{
private:
  bool expander_inited = false;
  bool sht_inited = false;
  std::tuple <float, float> sht_state[SHT_COUNT];

  ClosedCube_SHT31D sht;
  PCF8574 expander;
  int sht_pcf_pins[SHT_COUNT];
  int current_sht_idx;

  void init_expander()
  { 
    this->expander_inited = this->expander.begin();
    if (this->expander_inited)
    {
      this->sht_pcf_pins[0] = U1;
      this->sht_pcf_pins[1] = U2;
      this->sht_pcf_pins[2] = U3;
      this->current_sht_idx = 0;

      for (int i = 0; i < SHT_COUNT; i++)
      {
        pinMode(this->sht_pcf_pins[i], OUTPUT);
      }
      this->update_pcf_up_sht();
    }
  }

  void init_sht()
  {
    this->sht_inited = (this->sht.begin(I2C_SHT) == SHT3XD_NO_ERROR);
    if (this->sht_inited)
    {
      for (int i = 0; i < SHT_COUNT; i++)
      {
        this->sht_state[i] = std::make_tuple(std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
      }
    }
  }

  void update_pcf_up_sht()
  {
    for (int i = 0; i < SHT_COUNT; i++)
    {
      this->expander.write(sht_pcf_pins[i], this->current_sht_idx == i ? LOW : HIGH);
    }
    delay(10);
  }

  void rotate_sht()
  {
    this->current_sht_idx = (this->current_sht_idx + 1) % SHT_COUNT;
    this->update_pcf_up_sht();
  }

  void read_sht()
  {
    SHT31D sensor = this->sht.readTempAndHumidity(SHT3XD_REPEATABILITY_LOW, SHT3XD_MODE_CLOCK_STRETCH, 50);
    if(sensor.error == SHT3XD_NO_ERROR) {
      this->sht_state[this->current_sht_idx] = std::make_tuple(sensor.t, sensor.rh);
    }
    else
    {
      this->sht_state[this->current_sht_idx] = std::make_tuple(std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
      ESP_LOGW("multi_sht.read", "SHT31D error for sensor %d: %d", this->current_sht_idx + 1, sensor.error);
    }
  }

  float get_average_temperature()
  {
    return compute_average(std::get<0>(this->sht_state[0]), std::get<0>(this->sht_state[1]), std::get<0>(this->sht_state[2]));
  }

  float get_average_humidity()
  {
    return compute_average(std::get<1>(this->sht_state[0]), std::get<1>(this->sht_state[1]), std::get<1>(this->sht_state[2]));
  }

  void publish_states()
  {
    this->temperature_sensor1->publish_state(std::get<0>(this->sht_state[0]));
    this->humidity_sensor1->publish_state(std::get<1>(this->sht_state[0]));
    this->temperature_sensor2->publish_state(std::get<0>(this->sht_state[1]));
    this->humidity_sensor2->publish_state(std::get<1>(this->sht_state[1]));
    this->temperature_sensor3->publish_state(std::get<0>(this->sht_state[2]));
    this->humidity_sensor3->publish_state(std::get<1>(this->sht_state[2]));

    this->temperature_sensor->publish_state(this->get_average_temperature());
    this->humidity_sensor->publish_state(this->get_average_humidity());
  }
public:
  Sensor *temperature_sensor1 = new Sensor();
  Sensor *humidity_sensor1 = new Sensor();
  Sensor *temperature_sensor2 = new Sensor();
  Sensor *humidity_sensor2 = new Sensor();
  Sensor *temperature_sensor3 = new Sensor();
  Sensor *humidity_sensor3 = new Sensor();

  Sensor *temperature_sensor = new Sensor();
  Sensor *humidity_sensor = new Sensor();

  MultiplexedShtSensor() : PollingComponent(5 * 60 * 1000) {}

  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

  void setup() override
  {
    this->init_expander();
    this->init_sht();
  }

  void update() override
  {
    if (!this->expander_inited)
    {
      ESP_LOGE("multi_sht.init", "Could not init PCF extender");
    }

    if (!this->sht_inited)
    {
      ESP_LOGE("multi_sht.init", "Could not init SHT");
    }

    if (!(this->expander_inited && this->sht_inited))
    {
      return;
    }

    for (int i = 0; i < SHT_COUNT; i++)
    {
      this->read_sht();
      this->rotate_sht();
    }

    this->publish_states();
  }
};