import { clock } from '../lib/format'
import styles from './Ruler.module.css'

export function Ruler({ lastSync }: { lastSync: Date | null }) {
  return (
    <div className={styles.ruler}>
      <span className={styles.sync}>
        SYNC {clock(lastSync)}
        <i className={styles.caret} />
      </span>
    </div>
  )
}
