\ NUMBER GUESSING GAME
: GAME
  CLR
  " GUESS 1-100" . 
  RAND             \ Hook pushes the secret number
  BEGIN
    \ User inputs guess (manually via command line or KEY hook)
    DUP .          \ Debug: show secret (remove in final)
    = IF 
      " WIN!" . BEEP 
    ELSE 
      " TRY AGAIN" . 
    THEN
  AGAIN ;

GAME