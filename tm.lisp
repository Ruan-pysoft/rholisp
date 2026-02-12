(tmpfn
  (x)
  (if x (rec (print 1)) (print 0))
  ((prompt ' Enter ' 0 ' or ' 1 ' :))
)
