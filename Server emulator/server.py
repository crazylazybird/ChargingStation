import asyncio
from datetime import datetime, timezone

from websockets.server import serve
from websockets.exceptions import ConnectionClosedError

from ocpp.routing import on
from ocpp.v16 import ChargePoint as CP
from ocpp.v16 import call, call_result

# prompt_toolkit
from prompt_toolkit import PromptSession
from prompt_toolkit.patch_stdout import patch_stdout


CONNECTED_CPS = {}


# --------------------- CENTRAL SYSTEM CLASS -----------------------
class CentralSystem(CP):
    """
    Реализация CSMS для OCPP 1.6J.
    """

    async def send_call(self, action, payload):
        """
        Отправка произвольного CALL в зарядную станцию.
        action: строка с названием действия (например, "RemoteStartTransaction")
        payload: dict с параметрами.
        В ocpp>=1.x классы называются без суффикса Payload.
        """
        print(f"[SEND] Action={action}, Payload={payload}")

        req_cls = getattr(call, action)
        req = req_cls(**payload)

        response = await self.call(req)
        print(f"[RESP] {action} -> {response}")
        return response

    @on("BootNotification")
    async def on_boot_notification(self, charge_point_model, charge_point_vendor, **kwargs):
        print(f"[BOOT] vendor={charge_point_vendor}, model={charge_point_model}")
        return call_result.BootNotification(
            current_time=datetime.now(timezone.utc).isoformat(),
            interval=10,
            status="Accepted",
        )

    @on("Heartbeat")
    async def on_heartbeat(self):
        # Можно закомментировать, если шумит:
        # print("[HEARTBEAT] received.")
        return call_result.Heartbeat(
            current_time=datetime.now(timezone.utc).isoformat()
        )

    @on("StatusNotification")
    async def on_status_notification(self, connector_id, error_code, status, **kwargs):
        print(f"[STATUS] connector={connector_id}, status={status}, error={error_code}")
        return call_result.StatusNotification()

    @on("MeterValues")
    async def on_meter_values(self, connector_id, meter_value, **kwargs):
        print(f"[METER] {meter_value}")
        return call_result.MeterValues()

    @on("StartTransaction")
    async def on_start_transaction(self, connector_id, id_tag, meter_start, timestamp, **kwargs):
        print(f"[START] id_tag={id_tag}, meter={meter_start}")
        return call_result.StartTransaction(
            transaction_id=1,
            id_tag_info={"status": "Accepted"},
        )

    @on("StopTransaction")
    async def on_stop_transaction(self, meter_stop, timestamp, transaction_id, id_tag, **kwargs):
        print(f"[STOP] id_tag={id_tag}, meter={meter_stop}")
        return call_result.StopTransaction(
            id_tag_info={"status": "Accepted"},
        )


# --------------------------- HANDLER ------------------------------
async def on_connect(websocket, path):
    charge_point_id = path.strip("/")
    print(f"[CONNECT] new charge point connected: {charge_point_id}")

    cp = CentralSystem(charge_point_id, websocket)
    CONNECTED_CPS[charge_point_id] = cp

    try:
        await cp.start()
    except ConnectionClosedError as e:
        print(f"[DISCONNECT] {charge_point_id}: connection closed: {e}")
    except Exception as e:
        print(f"[ERROR] Handler for {charge_point_id} crashed: {e}")
    finally:
        CONNECTED_CPS.pop(charge_point_id, None)
        print(f"[CLEANUP] charge point {charge_point_id} removed from CONNECTED_CPS")


# ---------------------------- CLI (prompt_toolkit) ----------------
async def cli():
    """
    CLI на основе prompt_toolkit:
    - корректно перерисовывает строку ввода, даже когда приходят print'ы;
    - есть история команд (стрелка ↑).
    """
    session = PromptSession()

    # patch_stdout перехватывает print() и аккуратно перерисовывает prompt
    with patch_stdout():
        while True:
            try:
                cmd_line = await session.prompt_async(">>> ")
            except (EOFError, KeyboardInterrupt):
                print("Exiting...")
                raise SystemExit

            parts = cmd_line.strip().split()
            if not parts:
                continue

            cmd = parts[0].lower()

            if cmd in ("exit", "quit"):
                print("Exiting...")
                for cp_id in list(CONNECTED_CPS.keys()):
                    print(f"[CLOSE] Closing connection for {cp_id}")
                raise SystemExit

            elif cmd == "list":
                if not CONNECTED_CPS:
                    print("No connected charge points.")
                else:
                    print("Connected charge points:")
                    for cp_id in CONNECTED_CPS:
                        print(f" - {cp_id}")

            elif cmd == "start":
                # start <cp_id> <id_tag> <connector_id>
                if len(parts) < 4:
                    print("Usage: start <cp_id> <id_tag> <connector_id>")
                    continue
                cp_id = parts[1]
                id_tag = parts[2]
                try:
                    connector_id = int(parts[3])
                except ValueError:
                    print("connector_id must be integer.")
                    continue

                cp = CONNECTED_CPS.get(cp_id)
                if not cp:
                    print(f"Charge point {cp_id} not connected.")
                    continue

                payload = {
                    "id_tag": id_tag,
                    "connector_id": connector_id,
                }
                try:
                    response = await cp.send_call("RemoteStartTransaction", payload)
                    print(f"[RESULT] RemoteStartTransaction: {response}")
                except Exception as e:
                    print(f"[ERROR] RemoteStartTransaction failed: {e}")

            elif cmd == "stop":
                # stop <cp_id> <transaction_id>
                if len(parts) < 3:
                    print("Usage: stop <cp_id> <transaction_id>")
                    continue
                cp_id = parts[1]
                try:
                    transaction_id = int(parts[2])
                except ValueError:
                    print("transaction_id must be integer.")
                    continue

                cp = CONNECTED_CPS.get(cp_id)
                if not cp:
                    print(f"Charge point {cp_id} not connected.")
                    continue

                payload = {"transaction_id": transaction_id}
                try:
                    response = await cp.send_call("RemoteStopTransaction", payload)
                    print(f"[RESULT] RemoteStopTransaction: {response}")
                except Exception as e:
                    print(f"[ERROR] RemoteStopTransaction failed: {e}")

            elif cmd == "reset":
                # reset <cp_id>
                if len(parts) < 2:
                    print("Usage: reset <cp_id>")
                    continue
                cp_id = parts[1]
                cp = CONNECTED_CPS.get(cp_id)
                if not cp:
                    print(f"Charge point {cp_id} not connected.")
                    continue

                payload = {"type": "Soft"}
                try:
                    response = await cp.send_call("Reset", payload)
                    print(f"[RESULT] Reset: {response}")
                except Exception as e:
                    print(f"[ERROR] Reset failed: {e}")

            else:
                print("Unknown command. Available commands:")
                print("  list")
                print("  start <cp_id> <id_tag> <connector_id>")
                print("  stop <cp_id> <transaction_id>")
                print("  reset <cp_id>")
                print("  exit / quit")


# ----------------------- SERVER STARTUP ---------------------------
async def main():
    server = await serve(
        on_connect,
        "0.0.0.0",
        9000,
        subprotocols=["ocpp1.6"],  # важно для OCPP 1.6J
    )
    print("[SERVER] OCPP 1.6 CSMS listening on ws://0.0.0.0:9000/{ChargeBoxId}")

    try:
        await cli()
    finally:
        server.close()
        await server.wait_closed()
        print("[SERVER] stopped.")


if __name__ == "__main__":
    asyncio.run(main())
