#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client
{
	int	id;
	char	*buf;
}	t_client;

t_client	clis[1024];
int		sockfd;
int		max;
int		gid;
fd_set		set, rd_set, wr_set;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	free_buf(char **p)
{
	if(p && *p)
	{
		free(*p);
		*p = NULL;
	}
}

void	print(char *msg)
{
	write(1, msg, strlen(msg));
}

void    log_e(char *msg)
{
        write(2, msg, strlen(msg));
}

void	fatal(void)
{
	log_e("Fatal error\n");
}

void    wrong_args(void)
{
        log_e("Wrong number of arguments\n");
	exit(1);
}


void	pr_head(char *p, int cid, int mid)
{
	switch(mid)
	{
		case 0:
			sprintf(p, "server: client %d just arrived\n", cid);
			return ;
                case 1:
                        sprintf(p, "server: client %d just left\n", cid);
                        return ;
		default:
                        sprintf(p, "client %d: ", cid);
                        return ;
	}
}

void	init_clis()
{
        for (int fd = 0; fd < 1024; fd++)
        {
                clis[fd].buf = NULL;
                clis[fd].id = -1;
        }
	FD_ZERO(&set);
	FD_SET(sockfd, &set);
	gid = 0;
	max = sockfd;
}

void	clean_clis()
{
	for (int fd = 0; fd <= max; fd++)
	{
		free_buf(&(clis[fd].buf));
		if (clis[fd].id>=0)
			close(fd);
	}
}

void	fatal_exit()
{
	fatal();
	clean_clis();
	close(sockfd);
	exit(1);
}

void	send_all(int exc, char *head, char **p, size_t len)
{
	for (int fd = 3; fd <= max; fd++)
	{
		if (fd != exc && clis[fd].id>=0 && FD_ISSET(fd, &wr_set))
		{
			ssize_t	send_ret;
			if (( send_ret = send(fd, head, strlen(head), 0) ) < 0)
			{
				free_buf(p);
				fatal_exit();
			}
			if (p && *p && len>0)
			{
	                        if ( ( send_ret = send(fd, *p, len, 0) ) < 0)
	                        {
	                                free_buf(p);
	                                fatal_exit();
	                        }
			}
		}
	}
	free_buf(p);
}

void	cli_con(void)
{
        int			fd;
	socklen_t		len;
        struct sockaddr_in	cli;
	char			head[128];

        len = sizeof(cli);
        fd = accept(sockfd, (struct sockaddr *)&cli, &len);
        if (fd < 0) { 
                fatal(); 
                exit(1); 
        } 
	FD_SET(fd, &set);
	if (max < fd)
		max = fd;
	clis[fd].id = gid;
	gid++;
	pr_head(head, clis[fd].id, 0);
	send_all(fd, head, NULL, 0);
}

void	cli_disc(int fd)
{
        char                    head[128];
	int id = clis[fd].id;
	
	if(id>=0)
	{
		clis[fd].id = -1;
		FD_CLR(fd, &set);
		if (max==fd)
			max--;
		close(fd);
		
		pr_head(head, id, 1);
		send_all(fd, head, NULL, 0);
	}

}

void	append_buf(int fd, char *buf)
{
	char	*newbuf;

	newbuf = str_join(clis[fd].buf, buf);
	if (!newbuf)
		fatal_exit();
	clis[fd].buf = newbuf;
}

void	send_messages(int fd)
{
	char                    head[128];
	int			ext_ret;
	char			*ext_buf;

	pr_head(head, clis[fd].id, 2);

	while (  (ext_ret = extract_message(&(clis[fd].buf), &ext_buf )) )
	{
		if (ext_ret < 0)
			fatal_exit();
        	send_all(fd, head, &ext_buf, strlen(ext_buf));
	}
}



int main(int argc, char **argv) {
	struct sockaddr_in servaddr;

	if (argc != 2)
		wrong_args();

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fatal(); 
		exit(1); 
	} 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fatal(); 
		exit(1); 
	} 
	if (listen(sockfd, 4096) != 0) {
		fatal(); 
		exit(1); 
	}

	init_clis();
	while(1)
	{
		ssize_t	sel_ret;
		wr_set = set;
		rd_set = set;
		sel_ret = select(max+1, &rd_set, &wr_set, NULL, NULL);
		if (sel_ret <= 0)
			continue;
		if (FD_ISSET(sockfd, &rd_set))
		{
			cli_con();
			continue;
		}
		for (int fd = 3; fd <= max; fd++)
		{
			if ( sockfd!=fd && FD_ISSET(fd, &rd_set) && clis[fd].id>=0 )
			{
				ssize_t	recv_ret;
				char	recv_buf[4096];
				if ( (recv_ret = recv(fd, recv_buf, 4095, 0)) < 0)
					fatal_exit();
				if (recv_ret)
				{
					recv_buf[recv_ret]=0;
					append_buf(fd, recv_buf);
					send_messages(fd);
				}
				else
					cli_disc(fd);
			}
		}
	}
}
