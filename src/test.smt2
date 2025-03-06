(set-info :smt-lib-version 2.6)
(set-logic BV)
(set-info :status unsat)

(assert (exists ((x (_ BitVec 8))) (forall ((y (_ BitVec 8))) (= (bvmul x y) #x00) ) ))
(check-sat)
(exit)
