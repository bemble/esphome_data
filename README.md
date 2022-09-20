# esphome-data

> My ESPHome data.

- **garage-door:** enable the control of my front garage door 
- **gazpar:** get gaz consumption with french gaz meter "gazpar" (details [here, in french](./ressources/gazpar.md))
- **station-meteo:** `WIP` meteo station

## Fix secrets

In `common` folder, add a `secrets.yaml` file which starts with:

```yaml
<<: !include ../secrets.yaml
```