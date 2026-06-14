import { useEffect, useState } from 'react'
import { DeviceCard } from './components/DeviceCard'
import { Masthead } from './components/Masthead'
import { Ruler } from './components/Ruler'
import type { Device } from './types'
import styles from './App.module.css'

const API = 'http://localhost:8080/api/devices'

export default function App() {
  const [devices, setDevices] = useState<Device[]>([])
  const [serverOnline, setServerOnline] = useState(false)
  const [lastSync, setLastSync] = useState<Date | null>(null)

  useEffect(() => {
    let active = true

    async function load() {
      try {
        const res = await fetch(API)
        if (!res.ok) throw new Error(String(res.status))
        const data: Device[] = await res.json()
        if (!active) return
        setDevices(data)
        setServerOnline(true)
        setLastSync(new Date())
      } catch {
        if (active) setServerOnline(false)
      }
    }

    load()
    const id = setInterval(load, 2000)
    return () => {
      active = false
      clearInterval(id)
    }
  }, [])

  const sorted = [...devices].sort((a, b) =>
    a.device_id.localeCompare(b.device_id),
  )
  const onlineCount = devices.filter((d) => d.online).length

  return (
    <>
      <div className="scrim" />
      <div className={styles.hub}>
        <Masthead serverOnline={serverOnline} onlineCount={onlineCount} />
        <Ruler lastSync={lastSync} />

        {sorted.length === 0 ? (
          <div className={styles.void}>
            <span className={styles.voidMark} />
            {serverOnline ? 'NO NODES BROADCASTING' : 'AWAITING SERVER LINK'}
          </div>
        ) : (
          <main className={styles.rack}>
            {sorted.map((d, i) => (
              <DeviceCard key={d.device_id} device={d} index={i} />
            ))}
          </main>
        )}
      </div>
    </>
  )
}
