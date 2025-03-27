(set-info :smt-lib-version 2.6)
(set-logic BV)
(set-info :status unsat)
(declare-fun x () (_ BitVec 32))

(assert (= x #x00000000))
(check-sat)
(exit)
