import asyncio
from datetime import datetime, timezone

from websockets.server import serve
from ocpp.routing import on
from ocpp.v16 import ChargePoint as CP
from ocpp.v16 import call, call_result

# Глобальный флаг: можно ли заряжать (Accepted/Rejected)
ALLOW_CHARGE = True

# Подключённые зарядки: id -> объект CentralSystem
CONNECTED_CPS = {}

# Простой счётчик транзакций
TRANSACTION_COUNTER = 1


class CentralSystem(CP):
    def __init__(self, id, websocket):
        super().__init__(id, websocket)
        self.last_transaction_id = None

    # ====== ВХОДЯЩИЕ СООБЩЕНИЯ ОТ СТАНЦИИ ======

    @on("BootNotification")
    async def on_boot_notification(self, charge_point_vendor, charge_point_model, **kwargs):
        print(f"[BOOT] vendor={charge_point_vendor}, model={charge_point_model}, id={self.id}")
        return call_result.BootNotification(
            current_time=datetime.now(timezone.utc).isoformat(),
            interval=60,
            status="Accepted"
        )

    @on("Heartbeat")
    async def on_heartbeat(self):
        print(f"[HEARTBEAT] from {self.id}")
        return call_result.Heartbeat(
            current_time=datetime.now(timezone.utc).isoformat()
        )

    @on("StatusNotification")
    async def on_status_notification(self, connector_id, error_code, status, **kwargs):
        print(f"[STATUS] cp={self.id}, connector={connector_id}, status={status}, error={error_code}")
        return call_result.StatusNotification()

    @on("MeterValues")
    async def on_meter_values(self, connector_id, meter_value, **kwargs):
        print(f"[METER] cp={self.id}, connector={connector_id}, data={meter_value}")
        return call_result.MeterValues()

    @on("StartTransaction")
    async def on_start_transaction(self, connector_id, id_tag, meter_start, timestamp, **kwargs):
        global TRANSACTION_COUNTER, ALLOW_CHARGE
        status = "Accepted" if ALLOW_CHARGE else "Rejected"
        tx_id = TRANSACTION_COUNTER
        TRANSACTION_COUNTER += 1
        self.last_transaction_id = tx_id

        print(f"[START_TX] cp={self.id}, conn={connector_id}, idTag={id_tag}, "
              f"meterStart={meter_start}, status={status}, tx={tx_id}")

        return call_result.StartTransaction(
            id_tag_info={"status": status},
            transaction_id=tx_id
        )

    @on("StopTransaction")
    async def on_stop_transaction(self, transaction_id, meter_stop, timestamp, **kwargs):
        print(f"[STOP_TX] cp={self.id}, tx={transaction_id}, meterStop={meter_stop}")
        return call_result.StopTransaction(
            id_tag_info={"status": "Accepted"}
        )

    # ====== ИСХОДЯЩИЕ КОМАНДЫ К СТАНЦИИ ======

    async def remote_start(self, id_tag="remote-tag"):
        """Отправить RemoteStartTransaction на станцию."""
        try:
            req = call.RemoteStartTransaction(
                connector_id=1,
                id_tag=id_tag
            )
            print(f"[CSMS] Sending RemoteStartTransaction to {self.id} (idTag={id_tag})")
            res = await self.call(req)
            print(f"[CSMS] RemoteStartTransaction result for {self.id}: {res.status}")
        except Exception as e:
            print(f"[ERROR] RemoteStartTransaction to {self.id} failed: {e}")

    async def remote_stop(self):
        """Отправить RemoteStopTransaction на станцию (если знаем последний tx_id)."""
        if self.last_transaction_id is None:
            print(f"[CSMS] No last_transaction_id for {self.id}, nothing to stop")
            return

        try:
            req = call.RemoteStopTransaction(
                transaction_id=self.last_transaction_id
            )
            print(f"[CSMS] Sending RemoteStopTransaction to {self.id} (tx={self.last_transaction_id})")
            res = await self.call(req)
            print(f"[CSMS] RemoteStopTransaction result for {self.id}: {res.status}")
        except Exception as e:
            print(f"[ERROR] RemoteStopTransaction to {self.id} failed: {e}")


async def on_connect(websocket, path):
    charge_point_id = path.strip("/")
    print(f"[CONNECT] new charge point connected: {charge_point_id}")
    cp = CentralSystem(charge_point_id, websocket)
    CONNECTED_CPS[charge_point_id] = cp
    await cp.start()


async def cli_loop():
    """
    Команды:
      allow on        — сервер принимает StartTransaction (зарядка разрешена)
      allow off       — сервер отклоняет StartTransaction (зарядка запрещена)
      rs <CP_ID>      — RemoteStartTransaction для станции <CP_ID>
      rstop <CP_ID>   — RemoteStopTransaction для станции <CP_ID>
      list            — список подключённых станций
    """
    global ALLOW_CHARGE

    loop = asyncio.get_running_loop()
    while True:
        cmd = await loop.run_in_executor(None, input, "> ")

        parts = cmd.strip().split()
        if not parts:
            continue

        if parts[0] == "allow" and len(parts) == 2:
            if parts[1].lower() == "on":
                ALLOW_CHARGE = True
                print("[CLI] Charging permission: ON (StartTransaction -> Accepted)")
            elif parts[1].lower() == "off":
                ALLOW_CHARGE = False
                print("[CLI] Charging permission: OFF (StartTransaction -> Rejected)")
            else:
                print("Usage: allow on|off")

        elif parts[0] == "rs" and len(parts) >= 2:
            cp_id = parts[1]
            id_tag = parts[2] if len(parts) >= 3 else "remote-tag"
            cp = CONNECTED_CPS.get(cp_id)
            if cp:
                asyncio.create_task(cp.remote_start(id_tag))
            else:
                print(f"[CLI] Charge point {cp_id} not connected")

        elif parts[0] == "rstop" and len(parts) == 2:
            cp_id = parts[1]
            cp = CONNECTED_CPS.get(cp_id)
            if cp:
                asyncio.create_task(cp.remote_stop())
            else:
                print(f"[CLI] Charge point {cp_id} not connected")

        elif parts[0] == "list":
            if not CONNECTED_CPS:
                print("[CLI] No connected charge points")
            else:
                print("[CLI] Connected charge points:")
                for cid, cp in CONNECTED_CPS.items():
                    print(f"  - {cid} (last_tx={cp.last_transaction_id})")
        else:
            print("Commands:")
            print("  allow on|off")
            print("  rs <CP_ID> [idTag]")
            print("  rstop <CP_ID>")
            print("  list")


async def main():
    async with serve(on_connect, "0.0.0.0", 9000):
        print("[SERVER] OCPP 1.6 CSMS listening on ws://0.0.0.0:9000/{ChargeBoxId}")
        asyncio.create_task(cli_loop())
        await asyncio.Future()  # вечный цикл


if __name__ == "__main__":
    asyncio.run(main())
