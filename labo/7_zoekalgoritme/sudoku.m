function sudoku_stud( )
    % create random puzzles or look some up in de newspapers
    p = [reshape(randperm(9),3,3),reshape(randperm(9),3,3),reshape(randperm(9),3,3);
        reshape(randperm(9),3,3),reshape(randperm(9),3,3),reshape(randperm(9),3,3);
        reshape(randperm(9),3,3),reshape(randperm(9),3,3),reshape(randperm(9),3,3)];
    n = 40;
    blanks = randint(1,n,81)+1;
    p(blanks) = 0;
    randompuzzle = p;
      
    simple = [9, 0, 0, 6, 0, 4, 7, 0, 0;
          4, 5, 0, 0, 0, 3, 0, 0, 1;
          1, 0, 0, 8, 5, 0, 0, 6, 0;
          0, 0, 0, 3, 0, 7, 0, 4, 0;
          8, 0, 7, 0, 9, 2, 0, 0, 0;
          0, 0, 2, 0, 0, 5, 3, 0, 0;
          2, 0, 0, 0, 0, 0, 5, 0, 6;
          0, 6, 0, 0, 0, 8, 0, 0, 0;
          0, 1, 5, 9, 0, 0, 8, 0, 7];

    medium = [0, 0, 8, 0, 2, 0, 0, 0, 1;
          0, 0, 0, 0, 0, 1, 0, 0, 5;
          0, 2, 0, 9, 0, 7, 0, 4, 0;
          0, 0, 0, 8, 0, 0, 0, 5, 3;
          4, 0, 0, 1, 0, 0, 2, 0, 0;
          0, 6, 0, 4, 3, 9, 0, 8, 0;
          9, 7, 0, 0, 5, 0, 0, 0, 8;
          3, 0, 6, 0, 0, 0, 0, 9, 4;
          0, 0, 0, 0, 0, 0, 0, 0, 2];

    advanced = [0, 0, 8, 0, 0, 0, 0, 0, 0;
          0, 9, 2, 0, 0, 0, 4, 0, 0;
          0, 6, 0, 0, 3, 0, 0, 8, 0;
          0, 0, 0, 0, 0, 0, 0, 1, 5;
          0, 0, 6, 0, 0, 1, 3, 0, 0;
          0, 0, 4, 7, 0, 5, 0, 0, 2;
          0, 0, 0, 0, 0, 7, 0, 9, 0;
          0, 0, 0, 0, 9, 0, 7, 4, 1;
          0, 5, 0, 3, 0, 0, 0, 0, 0];


    % Choose a puzzle
    puzzle = advanced;

    disp('Opgave: ')
    disp(puzzle);
    
    % zoek de lege velden
    [es_x,es_y] = find(puzzle == 0);
    disp('Occupancy ratio: '),(81-length(es_x))/81;

    % cre�er een lijst met alle mogelijkheden voor elk leeg veld in de set
    % allpos:
    allpos= {};
    for i = 1:length(es_x),
        allpos = {allpos{1:end}, listpossibilities(puzzle,es_x(i),es_y(i))};
    end

    % How many configuration are still possible for this puzzle?
    disp('Number of configurations to be verified in worst case: ')
    len = 1;
    for i = 1:length(allpos),
        len = len*length(allpos{i});
    end
    display(len);

    % Solve the puzzle:
    confcount = 0;
    itemstoprocess = zeros(9,9,721);%aanmaken lege array van 9x9 matrices, voor alle mogelijke tussenoplossingen
    itemstoprocess(:,:,1) = reshape(puzzle(:),9,9);
    possibleSolutions = 1;

    solved = 0;
    while ~solved,
        % If no children are left, stop the search
        if possibleSolutions == 0,
            disp('Geen oplossing mogelijk.');
            disp('Aantal berekende oplossingen');
            disp(confcount);
            break;
        else
            % else get the last element out of the list and compute the new
            % children and add them to the list
            current = itemstoprocess(:,:,possibleSolutions);            
            % verwijderne eerste element
            itemstoprocess(:,:,possibleSolutions) = [];
            % eerste element verwijdert dus 1 mogelijkheid minder
            possibleSolutions = possibleSolutions - 1;
            %updaten teller gebruikte oplossingen
            confcount = confcount + 1;
            % alle indices zoeken waar een 0 nog staat
            % eerste index waar nog een nul staat, er is zowiezo nog een
            % lege plek want anders hadden we een oplossing gevonden
            firstZero = find(current == 0, 1 );
            
            % we gaan er van uit dat vorige oplossing wel mogelijk
            % was, dus enkel nood om de rij/kolom waar we gewijzigd
            % hebben te controleren + blokje
            colIndex = floor((firstZero-1)/9);
            rowIndex = mod(firstZero-1, 9)+1;
            
            % bepalen start index kotje waarin we zitten
            startIndex = (floor((rowIndex-1)/3))*3+1 + floor((colIndex)/3)*27;
       
            % een rij om te zien welke getallen we nog kunne invullen
            mogelijk = zeros(1, 10);
                
            %rijen overlopen
            for k = 0:8,
                mogelijk(current(rowIndex+k*9)+1) = 1;
            end
                
            % kolommen overlopen
            for k = 1:9,
                mogelijk(current(k+colIndex*9)+1) = 1;
            end

            % nu het blokje checken
            for k = 0:2,
                for l = 0:2,
                    mogelijk(current(startIndex+l*9+k)+1) = 1;
                end  
            end
          
            for i = 1:9,
                % de 0 vervangen door getal 1 tem 9
                if mogelijk(i+1) == 0,
                    current(firstZero) = i;
                    if isempty(find(current == 0, 1)),
                        disp('Gevonden oplossing');
                        disp(current);
                        disp('Aantal berekende oplossingen');
                        disp(confcount);
                        solved = 1;
                        break;
                    % Nog lege plaatsen dus toevoegen aan mogelijkheden
                    else
                        possibleSolutions = possibleSolutions + 1;
                        itemstoprocess(:,:,possibleSolutions) = current;
                    end
                end
            end    
        end
    end
end

% easy
% Gevonden oplossing
%      9     2     8     6     1     4     7     5     3
%      4     5     6     2     7     3     9     8     1
%      1     7     3     8     5     9     4     6     2
%      5     9     1     3     6     7     2     4     8
%      8     3     7     4     9     2     6     1     5
%      6     4     2     1     8     5     3     7     9
%      2     8     4     7     3     1     5     9     6
%      7     6     9     5     2     8     1     3     4
%      3     1     5     9     4     6     8     2     7
% 
% Aantal berekende oplossingen: 185

% medium
% Gevonden oplossing
%      6     4     8     5     2     3     9     7     1
%      7     3     9     6     4     1     8     2     5
%      5     2     1     9     8     7     3     4     6
%      1     9     7     8     6     2     4     5     3
%      4     8     3     1     7     5     2     6     9
%      2     6     5     4     3     9     1     8     7
%      9     7     2     3     5     4     6     1     8
%      3     5     6     2     1     8     7     9     4
%      8     1     4     7     9     6     5     3     2
% 
% Aantal berekende oplossingen: 608
        
% advanced
% Gevonden oplossing
%      4     3     8     1     7     9     2     5     6
%      1     9     2     8     5     6     4     3     7
%      7     6     5     4     3     2     1     8     9
%      2     7     9     6     4     3     8     1     5
%      5     8     6     9     2     1     3     7     4
%      3     1     4     7     8     5     9     6     2
%      8     4     1     2     6     7     5     9     3
%      6     2     3     5     9     8     7     4     1
%      9     5     7     3     1     4     6     2     8
% 
% Aantal berekende oplossingen: 91305