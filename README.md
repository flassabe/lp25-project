# Exemple d'implémentation du projet

Cet exemple n'est pas un absolu, certaines choses sont améliorables (robustesse en vérifiant tous les retours de fonctions, optimisation notamment du reducer final qui prend dans les 2 minutes à s'exécuter, etc.)

Une nouvelle option a été ajoutée : `-m` prenant un des 3 arguments suivants : `fifo`, `direct`, `mq`; et permettant de choisir la méthode de gestion des processus.

Ce projet va aussi être donné à des étudiants en FISA informatique, mais pour une conception et une implémentation cette fois en C++.

# Branche solution-cpp-reducer

Dans cette branche, le reducer est écrit en C++ pour permettre l'utilisation de la structure de données std::map. Les map sont des conteneurs associatifs qui indexent leur contenu par clé (ici un type string contenant un e-mail). La valeur correspondant à l'index est unique mais peut être un objet (i.e. un type de données complexe, c'est-à-dire une instance de classe, notion que vous verrez en LP2A/B (TC) ou en AP4A (INFO1)). Dans le cas de cette solution, une première map indexe des listes de destinataires avec les expéditeurs. Les destinataires sont également contenus dans une map qui indexe les occurrences de chaque destinataire avec leur e-mail.

L'intérêt des map est de se baser sur des arbres binaires pour leur stockage, avec un temps de recherche en O(log2(N)) très efficace. Pour la comparaison, mon implémentation de reducer en C qui se base sur des listes triées pour accélérer la recherche et l'ajout prend environ 2 minutes à analyser le fichier step2_output, alors que la version C++ avec les map prend 2 secondes (60 fois plus rapide).

Ce test montre l'intérêt du choix des structures de données dans l'implémentation d'un algorithme.
