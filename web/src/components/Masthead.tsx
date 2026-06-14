import { pad } from '../lib/format'
import { useCountUp } from '../lib/useCountUp'
import styles from './Masthead.module.css'

type Props = {
  serverOnline: boolean
  onlineCount: number
}

export function Masthead({ serverOnline, onlineCount }: Props) {
  const shown = useCountUp(onlineCount)
  const ticking = shown !== onlineCount

  return (
    <header className={styles.masthead}>
      <div className={styles.mark}>
        <div className={styles.glyph}>
          <span />
          <span />
          <span />
          <span />
        </div>
        <div className={styles.title}>
          <h1>
            SENSOR<em>/</em>HUB
          </h1>
          <p>ESP32 · MQTT TELEMETRY GATEWAY</p>
        </div>
      </div>

      <div className={styles.readout}>
        <div className={`${styles.signal} ${serverOnline ? styles.live : styles.dead}`}>
          <span className={styles.bulb} />
          {serverOnline ? 'LIVE' : 'NO SIGNAL'}
        </div>
        <div className={styles.count}>
          <strong className={ticking ? styles.ticking : ''}>{pad(shown)}</strong>
          <span>
            NODES
            <br />
            ONLINE
          </span>
        </div>
      </div>
    </header>
  )
}
