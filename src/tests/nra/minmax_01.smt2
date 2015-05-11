(set-logic QF_NRA_ODE)
(declare-fun d () Real)
(declare-fun d_0_0 () Real)  (declare-fun d_0_t () Real)
(declare-fun d1_0_t () Real) (declare-fun d2_0_t () Real)
(declare-fun time_0 () Real) (declare-fun mode_0 () Real)
(declare-fun d_1_0 () Real)  (declare-fun d_1_t () Real)
(declare-fun d1_1_t () Real) (declare-fun d2_1_t () Real)
(declare-fun time_1 () Real) (declare-fun mode_1 () Real)
(define-ode flow_1 ((= d/dt[d] 1)))
(define-ode flow_2 ((= d/dt[d] -1)))
(assert (<= -10 d_0_0))  (assert (<= d_0_0 10))
(assert (<= -10 d_0_t))  (assert (<= d_0_t 10))
(assert (<= -10 d1_0_t)) (assert (<= d1_0_t 10))
(assert (<= -10 d2_0_t)) (assert (<= d2_0_t 10))
(assert (<= 0 time_0 [0.000000])) (assert (<= time_0 1 [0.000000]))
(assert (<= 1 mode_0)) (assert (<= mode_0 1))
(assert (<= -10 d_1_0))  (assert (<= d_1_0 10))
(assert (<= -10 d_1_t))  (assert (<= d_1_t 10))
(assert (<= -10 d1_1_t)) (assert (<= d1_1_t 10))
(assert (<= -10 d2_1_t)) (assert (<= d2_1_t 10))
(assert (<= 0 time_1 [0.000000])) (assert (<= time_1 1 [0.000000]))
(assert (<= 1 mode_1)) (assert (<= mode_1 1))
(assert (and (= d_0_0 0) (= mode_0 1) 
             (= [d1_0_t] (integral 0. time_0 [d_0_0] flow_1)) 
             (= [d2_0_t] (integral 0. time_0 [d_0_0] flow_2)) 
             (>= d_0_t (min d1_0_t d2_0_t))
             (<= d_0_t (max d1_0_t d2_0_t))
             (= d_1_0 d_0_t) (= mode_1 1)
             (= [d1_1_t] (integral 0. time_1 [d_1_0] flow_1)) 
             (= [d2_1_t] (integral 0. time_1 [d_1_0] flow_2)) 
             (>= d_1_t (min d1_1_t d2_1_t))
             (<= d_1_t (max d1_1_t d2_1_t))
             (> d_1_t 1.99)))
(check-sat)
(exit)
