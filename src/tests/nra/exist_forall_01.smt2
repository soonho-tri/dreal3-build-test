(set-logic QF_NRA)
(declare-fun x1 () Real)
(declare-fun forall x2 () Real[7.0, 50.0])
(assert (<= -3.0 x1))
(assert (<= x1 3.14))
(assert (< (* x1 x2) 0))
(check-sat)
(exit)