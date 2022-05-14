#include "esphome.h"
#include "ClosedCube_SHT31D.h"
#include "PCF8574.h"

#define MSHT_TAG "multi_sht"

#define SHT_COUNT 3
#define MSHT_U1 2
#define MSHT_U2 1
#define MSHT_U3 3

#define I2C_SHT 0x44

float get_sum(float data[])
{
  float sum = 0.0;
  for(int i = 0; i < SHT_COUNT; ++i)
  {
    sum += data[i];
  }
  return sum;
}

float get_mean(float data[])
{
  return get_sum(data) / SHT_COUNT;
}

float get_standard_deviation(float data[])
{
  float mean = get_mean(data);
  float standard_deviation = .0;

  for(int i = 0; i < SHT_COUNT; ++i)
  {
    standard_deviation += pow(data[i] - mean, 2);
  }

  return sqrt(standard_deviation / SHT_COUNT);
}

/**
 * Compute average of given array.
 * If the deviation of the value is too high, the value will be removed
 */
float average(float data[])
{
  float sum = get_sum(data);
  float mean = get_mean(data);
  float std_dev = get_standard_deviation(data);

  int removed_count = 0;
  for(int i = 0; i < SHT_COUNT; ++i)
  {
    if(abs(data[i] - mean) > std_dev * 2)
    {
      sum -= data[i];
      removed_count ++;
    }
  }
  return sum / (SHT_COUNT - removed_count);
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
      this->sht_pcf_pins[0] = MSHT_U1;
      this->sht_pcf_pins[1] = MSHT_U2;
      this->sht_pcf_pins[2] = MSHT_U3;
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
      ESP_LOGW(MSHT_TAG, "SHT31D error for sensor %d: %d", this->current_sht_idx + 1, sensor.error);
    }
  }

  float get_average_temperature()
  {
    float data[SHT_COUNT];
    for(int i = 0; i < SHT_COUNT; ++i)
    {
      data[i] = std::get<0>(this->sht_state[i]);
    }
    return average(data);
  }

  float get_average_humidity()
  {
    float data[SHT_COUNT];
    for(int i = 0; i < SHT_COUNT; ++i)
    {
      data[i] = std::get<1>(this->sht_state[i]);
    }
    return average(data);
  }

  void publish_states()
  {
    for (int i = 0; i < SHT_COUNT; i++)
    {
      this->raw_temperature_sensors[i]->publish_state(std::get<0>(this->sht_state[i]));
      this->raw_humidity_sensors[i]->publish_state(std::get<1>(this->sht_state[i]));
    }

    this->temperature_sensor->publish_state(this->get_average_temperature());
    this->humidity_sensor->publish_state(this->get_average_humidity());
  }
public:
  // TODO: find a way to iterate with SHT_COUNT instead of hard add 3 sensors
  Sensor *raw_temperature_sensors[SHT_COUNT] = {new Sensor(), new Sensor(), new Sensor()};
  Sensor *raw_humidity_sensors[SHT_COUNT] = {new Sensor(), new Sensor(), new Sensor()};

  Sensor *temperature_sensor = new Sensor();
  Sensor *humidity_sensor = new Sensor();

  MultiplexedShtSensor() : PollingComponent(60 * 1000) {}

  float get_setup_priority() const override { return esphome::setup_priority::BUS; }

  void setup() override
  {
    this->init_expander();
    this->init_sht();
  }

  void update() override
  {
    if (!this->expander_inited)
    {
      ESP_LOGE(MSHT_TAG, "Could not init PCF extender");
    }

    if (!this->sht_inited)
    {
      ESP_LOGE(MSHT_TAG, "Could not init SHT");
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