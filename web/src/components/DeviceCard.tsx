import type { Device } from '../types'
import { pad } from '../lib/format'
import styles from './DeviceCard.module.css'

type Props = {
  device: Device
  index: number
}

export function DeviceCard({ device, index }: Props) {
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
