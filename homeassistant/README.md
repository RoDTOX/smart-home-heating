# Smart Heating - Configurare & Rulare Home Assistant

Acest folder conține configurarea completă Home Assistant pentru sistemul **Smart Heating** (ESP32 `192.168.1.20`), gata de rulat local în Docker/WSL sau pe un server dedicat.

---

## 📁 Structură Fișiere

- `configuration.yaml` - Configurația de bază (metric, timezone București, `mobile_app`, `packages`).
- `packages/smart_heating.yaml` - Integrarea REST completă: 3 entități Climate, 46 comutatoare/butoane, senzori de gaz, diagnoză DTC, protecție îngheț.
- `dashboards/smart_heating.yaml` - Tabloul de bord dedicat Lovelace (termostate, carduri cameră, diagnoză cazan).
- `docker-compose.yml` - Pornire rapidă a containerului Home Assistant cu rețea host.

---

## 🚀 Pornire Rapidă (Docker / WSL2)

Din acest director (`homeassistant/`), rulați:

```bash
docker compose up -d
```

Sau direct cu Docker CLI:

```bash
docker run -d \
  --name homeassistant \
  --privileged \
  --restart unless-stopped \
  -e TZ=Europe/Bucharest \
  -v $(pwd):/config \
  --net=host \
  homeassistant/home-assistant:stable
```

Home Assistant va fi disponibil imediat la adresa: `http://localhost:8123` (sau IP-ul mașinii în rețea).
