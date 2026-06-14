import { useEffect, useState } from 'react'
import type { Device } from '../types'
import { pad } from '../lib/format'
import styles from './DeviceCard.module.css'

type Props = {
  device: Device
  index: number
  onToggle: (id: string, component: string, value: number) => void
}

export function DeviceCard({ device, index, onToggle }: Props) {
  const actual = device.states?.onboard_led === 1
  const [optimistic, setOptimistic] = useState<boolean | null>(null)
  const ledOn = optimistic ?? actual

  useEffect(() => {
    if (optimistic !== null && actual === optimistic) {
      setOptimistic(null)
    }
  }, [actual, optimistic])

  const toggleLed = () => {
    const next = !ledOn
    setOptimistic(next)
    onToggle(device.device_id, 'onboard_led', next ? 1 : 0)
  }

  return (
    <article
      className={`${styles.node} ${device.online ? '' : styles.offline}`}
      style={{ animationDelay: `${index * 60}ms` }}
    >
      <div className={styles.top}>
        <span className={styles.ch}>CH {pad(index + 1)}</span>
        <span className={styles.state}>
          <span className={styles.pulse} />
          {device.online ? 'LIVE' : 'OFFLINE'}
        </span>
      </div>

      <h2 className={styles.id}>{device.device_id}</h2>

      <dl className={styles.specs}>
        <Row k="IPV4" v={device.ip} />
        <Row k="SSID" v={device.ssid} />
        <Row k="MAC" v={device.mac} />
      </dl>

      <div className={styles.control}>
        <span className={styles.control_label}>ONBOARD LED</span>
        <button
          type="button"
          className={`${styles.toggle} ${ledOn ? styles.toggle_on : ''}`}
          disabled={!device.online}
          aria-pressed={ledOn}
          onClick={toggleLed}
        >
          <span className={styles.knob} />
        </button>
      </div>
    </article>
  )
}

function Row({ k, v }: { k: string; v: string }) {
  return (
    <div className={styles.row}>
      <dt>{k}</dt>
      <span className={styles.lead} />
      <dd>{v || '—'}</dd>
    </div>
  )
}
