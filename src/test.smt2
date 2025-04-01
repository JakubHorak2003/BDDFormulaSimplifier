(set-info :smt-lib-version 2.6)
(set-logic BV)
(set-info :status sat)

(declare-const x (_ BitVec 32))
(declare-const y (_ BitVec 32))

(assert (not (= x y)))
(check-sat)