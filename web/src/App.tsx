import { useEffect, useState } from 'react'
import './App.css'

const API = 'http://localhost:8080/api/devices'

type Device = {
  device_id: string
  mac: string
  ip: string
  ssid: string
}

const pad = (n: number) => n.toString().padStart(2, '0')

function clock(d: Date | null) {
  if (!d) return '--:--:--'
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

export default function App() {
  const [devices, setDevices] = useState<Device[]>([])
  const [online, setOnline] = useState(false)
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
        setOnline(true)
        setLastSync(new Date())
      } catch {
        if (active) setOnline(false)
      }
    }

    load()
    const id = setInterval(load, 2000)
    return () => {
      active = false
      clearInterval(id)
    }
  }, [])

  return (
    <>
      <div className="scrim" />
      <div className="hub">
        <header className="masthead">
          <div className="mark">
            <div className="glyph">
              <span />
              <span />
              <span />
              <span />
            </div>
            <div className="title">
              <h1>
                SENSOR<em>/</em>HUB
              </h1>
              <p>ESP32 · MQTT TELEMETRY GATEWAY</p>
            </div>
          </div>

          <div className="readout">
            <div className={`signal ${online ? 'live' : 'dead'}`}>
              <span className="bulb" />
              {online ? 'LIVE' : 'NO SIGNAL'}
            </div>
            <div className="count">
              <strong>{pad(devices.length)}</strong>
              <span>
                NODES
                <br />
                ONLINE
              </span>
            </div>
          </div>
        </header>

        <div className="ruler">
          <span className="sync">
            SYNC {clock(lastSync)}
            <i className="caret" />
          </span>
        </div>

        {devices.length === 0 ? (
          <div className="void">
            <span className="void-mark" />
            {online ? 'NO NODES BROADCASTING' : 'AWAITING SERVER LINK'}
          </div>
        ) : (
          <main className="rack">
            {devices.map((d, i) => (
              <Node key={d.device_id} device={d} index={i} />
            ))}
          </main>
        )}
      </div>
    </>
  )
}

function Node({ device, index }: { device: Device; index: number }) {
  return (
    <article className="node" style={{ animationDelay: `${index * 60}ms` }}>
      <div className="node-top">
        <span className="ch">CH {pad(index + 1)}</span>
        <span className="pulse" />
      </div>
      <h2 className="id">{device.device_id}</h2>
      <dl className="specs">
        <Row k="IPV4" v={device.ip} />
        <Row k="SSID" v={device.ssid} />
        <Row k="MAC" v={device.mac} />
      </dl>
    </article>
  )
}

function Row({ k, v }: { k: string; v: string }) {
  return (
    <div className="row">
      <dt>{k}</dt>
      <span className="lead" />
      <dd>{v || '—'}</dd>
    </div>
  )
}
