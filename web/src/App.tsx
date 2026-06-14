import { useCallback, useEffect, useState } from "react";
import { DeviceCard } from "./components/DeviceCard";
import { Masthead } from "./components/Masthead";
import { Ruler } from "./components/Ruler";
import { getDevices, sendCommand } from "./lib/api";
import type { Device } from "./types";
import styles from "./App.module.css";

export default function App() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [serverOnline, setServerOnline] = useState(false);
  const [lastSync, setLastSync] = useState<Date | null>(null);

  const refresh = useCallback(async () => {
    try {
      const data = await getDevices();
      setDevices(data);
      setServerOnline(true);
      setLastSync(new Date());
    } catch {
      setServerOnline(false);
    }
  }, []);

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, 2000);
    return () => clearInterval(id);
  }, [refresh]);

  const handleToggle = useCallback(
    async (id: string, component: string, value: number) => {
      try {
        await sendCommand(id, component, value);
      } catch {
        /* command failed — optimistic reverts on next poll */
      }
    },
    [],
  );

  const sorted = [...devices].sort((a, b) =>
    a.device_id.localeCompare(b.device_id),
  );
  const onlineCount = devices.filter((d) => d.online).length;

  return (
    <>
      <div className="scrim" />
      <div className={styles.hub}>
        <Masthead serverOnline={serverOnline} onlineCount={onlineCount} />
        <Ruler lastSync={lastSync} />

        {sorted.length === 0 ? (
          <div className={styles.void}>
            <span className={styles.void_mark} />
            {serverOnline ? "NO NODES BROADCASTING" : "AWAITING SERVER LINK"}
          </div>
        ) : (
          <main className={styles.rack}>
            {sorted.map((d, i) => (
              <DeviceCard
                key={d.device_id}
                device={d}
                index={i}
                onToggle={handleToggle}
              />
            ))}
          </main>
        )}
      </div>
    </>
  );
}
