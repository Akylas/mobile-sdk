/** Renders `backticked` spans of a plain data string as <code>. No other markdown. */
export default function Ticked({children}) {
  if (typeof children !== 'string') return children;
  return (
    <>
      {children.split('`').map((part, i) =>
        i % 2 ? <code key={i}>{part}</code> : <span key={i}>{part}</span>,
      )}
    </>
  );
}
